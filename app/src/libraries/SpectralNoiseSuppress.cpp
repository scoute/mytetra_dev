#include "SpectralNoiseSuppress.h"

#include <QtMath>
#include <QtCore>

#include <cstring>

// Пассивные константы алгоритма
static const int NOISE_WIN_FRAMES=375; // окно пола шума ~2 с (187.5 кадр/с):
                                       // холды до пары секунд проходят,
                                       // фон в паузах давится как раньше
static const double SMOOTH_FACTOR=0.9; // сглаживание огибающей бина
static const double FLOOR_BIAS=1.2;    // множитель минимума огибающей
static const double SUBTRACT_BETA=2.0; // во сколько раз вычитается пол
static const double GAIN_FLOOR=0.05;   // предел гейна, -26 дБ
static const int SEED_FRAMES=32;       // засев пола: ~170 мс старта записи
static const int SEED_REPLICATE_AT=8;  // реплика пишется не первым кадром,
                                       // а девятым: окна к этому моменту
                                       // наполнены реальным звуком

SpectralNoiseSuppress::SpectralNoiseSuppress(void) :
    sampleRate(48000),
    frameSize(0),
    hopSize(0),
    noiseWin(NOISE_WIN_FRAMES),
    ringPos(0),
    seedLeft(0)
{
}

SpectralNoiseSuppress::~SpectralNoiseSuppress(void)
{
}


void SpectralNoiseSuppress::setup(int rate)
{
    sampleRate=rate;
    frameSize=512;
    hopSize=frameSize/2;
    ringPos=0;
    seedLeft=SEED_FRAMES;

    sqrtHann.assign(frameSize, 0.0);
    for(int i=0; i<frameSize; ++i)
        sqrtHann[i]=sqrt(0.5*(1.0-cos(2.0*M_PI*i/frameSize)));

    cosTable.assign(frameSize/2, 0.0);
    sinTable.assign(frameSize/2, 0.0);
    for(int i=0; i<frameSize/2; ++i)
    {
        cosTable[i]=cos(2.0*M_PI*i/frameSize);
        sinTable[i]=sin(2.0*M_PI*i/frameSize);
    }

    bitRev.assign(frameSize, 0);
    for(int i=0; i<frameSize; ++i)
    {
        int r=0, x=i;

        for(int b=0; b<9; ++b)
        {
            r=(r<<1)|(x&1);
            x>>=1;
        }

        bitRev[i]=r;
    }

    fftRe.assign(frameSize, 0.0);
    fftIm.assign(frameSize, 0.0);

    smoothMag.assign(frameSize/2+1, 0.0);
    noiseRing.assign((frameSize/2+1)*noiseWin, 0.0f);
    noiseFloor.assign(frameSize/2+1, 0.0);
    prevGain.assign(frameSize/2+1, 1.0);

    // Прогон латентности: на старте в потоке hop нулей, чтобы
    // первый настоящий кадр обработался с полным окном
    inRing.assign(hopSize, 0.0);
    outQ.clear();
    olaBuf.assign(frameSize, 0.0);
}


void SpectralNoiseSuppress::process(short *pcm, int samples)
{
    if(samples<=0)
        return;

    for(int i=0; i<samples; ++i)
        inRing.push_back(pcm[i]);

    while(inRing.size()>=static_cast<size_t>(frameSize))
        processFrame();

    // Возврат ровно samples сэмплов: нехватка (стартовый прогон
    // латентности) выдается нулями
    for(int i=0; i<samples; ++i)
    {
        double v=0.0;

        if(!outQ.empty())
        {
            v=outQ.front();
            outQ.pop_front();
        }

        pcm[i]=static_cast<short>(qBound(-32768.0, v, 32767.0));
    }
}


void SpectralNoiseSuppress::processFrame(void)
{
    const int half=frameSize/2;
    const int nBins=half+1;

    // Окно анализа кадра
    for(int i=0; i<frameSize; ++i)
    {
        fftRe[i]=inRing[i]*sqrtHann[i];
        fftIm[i]=0.0;
    }

    // Прямое преобразование (radix-2)
    for(int i=0; i<frameSize; ++i)
    {
        const int j=bitRev[i];

        if(j>i)
        {
            double t=fftRe[i]; fftRe[i]=fftRe[j]; fftRe[j]=t;
            t=fftIm[i]; fftIm[i]=fftIm[j]; fftIm[j]=t;
        }
    }

    for(int len=2; len<=frameSize; len<<=1)
    {
        const int halfLen=len>>1;
        const int tableStep=frameSize/len;

        for(int i=0; i<frameSize; i+=len)
        {
            for(int k=0; k<halfLen; ++k)
            {
                const int i1=i+k, i2=i+k+halfLen;
                const double wRe=cosTable[k*tableStep];
                const double wIm=-sinTable[k*tableStep];
                const double tr=fftRe[i2]*wRe-fftIm[i2]*wIm;
                const double ti=fftRe[i2]*wIm+fftIm[i2]*wRe;
                fftRe[i2]=fftRe[i1]-tr;
                fftIm[i2]=fftIm[i1]-ti;
                fftRe[i1]+=tr;
                fftIm[i1]+=ti;
            }
        }
    }

    // Пол по бинам: минимум сглаженной огибающей за последнее
    // время с множителем FLOOR_BIAS (компенсация систематического
    // занижения минимума). Кольцо читается целиком,
    // O(nBins*noiseWin) ~ 48k сравнений на кадр
    if(++ringPos>=noiseWin)
        ringPos=0;

    for(int k=0; k<nBins; ++k)
    {
        const double mag=sqrt(fftRe[k]*fftRe[k]+fftIm[k]*fftIm[k]);
        const float sm=(float)(SMOOTH_FACTOR*smoothMag[k]
                               +(1.0-SMOOTH_FACTOR)*mag);
        smoothMag[k]=sm;

        // Начальный засев: девятый кадр записывается по всему кольцу,
        // следующие - обновляют минимумом. Реплика не первым кадром:
        // первые кадры почти тихие из-за нулевого префилла латентности,
        // иначе пол весь период окна стоял бы near zero и подавления
        // в начале не было бы вообще. Агрессивный минимум засева
        // случайный всплеск в кадре реплики прибивает за пару кадров
        if(seedLeft>0)
        {
            float *col=noiseRing.data()+k*noiseWin;

            if(seedLeft==SEED_FRAMES-SEED_REPLICATE_AT)
            {
                for(int i=0; i<noiseWin; ++i)
                    col[i]=sm;
            }
            else
            {
                for(int i=0; i<noiseWin; ++i)
                    if(sm<col[i])
                        col[i]=sm;
            }
        }
        else
            noiseRing[k*noiseWin+ringPos]=sm;

        const float *col=noiseRing.data()+k*noiseWin;
        float mn=col[0];

        for(int i=1; i<noiseWin; ++i)
            if(col[i]<mn)
                mn=col[i];

        noiseFloor[k]=FLOOR_BIAS*(double)mn;

        // Вычитание величины шума из величины бина, оценка гейна
        double g=1.0;

        if(mag>0)
        {
            const double clean=qMax(mag-SUBTRACT_BETA*noiseFloor[k], 0.0);
            g=qMax(clean/mag, GAIN_FLOOR);
        }

        // Сглаживание: спад гейна не быстрее -8 дБ за кадр, чтобы
        // хвосты слов не обрезались
        g=qMax(g, prevGain[k]*0.4);
        prevGain[k]=g;

        fftRe[k]*=g;
        fftIm[k]*=g;

        // Зеркальная половина спектра для вещественного сигнала
        if(k>0 && k<half)
        {
            fftRe[frameSize-k]*=g;
            fftIm[frameSize-k]*=g;
        }
    }

    // Обратное преобразование через обмен re/im до и после
    for(int i=0; i<frameSize; ++i)
    {
        double t=fftRe[i]; fftRe[i]=fftIm[i]; fftIm[i]=t;
    }

    for(int i=0; i<frameSize; ++i)
    {
        const int j=bitRev[i];

        if(j>i)
        {
            double t=fftRe[i]; fftRe[i]=fftRe[j]; fftRe[j]=t;
            t=fftIm[i]; fftIm[i]=fftIm[j]; fftIm[j]=t;
        }
    }

    for(int len=2; len<=frameSize; len<<=1)
    {
        const int halfLen=len>>1;
        const int tableStep=frameSize/len;

        for(int i=0; i<frameSize; i+=len)
        {
            for(int k=0; k<halfLen; ++k)
            {
                const int i1=i+k, i2=i+k+halfLen;
                const double wRe=cosTable[k*tableStep];
                const double wIm=-sinTable[k*tableStep];
                const double tr=fftRe[i2]*wRe-fftIm[i2]*wIm;
                const double ti=fftRe[i2]*wIm+fftIm[i2]*wRe;
                fftRe[i2]=fftRe[i1]-tr;
                fftIm[i2]=fftIm[i1]-ti;
                fftRe[i1]+=tr;
                fftIm[i1]+=ti;
            }
        }
    }

    for(int i=0; i<frameSize; ++i)
    {
        double t=fftRe[i]; fftRe[i]=fftIm[i]; fftIm[i]=t;
    }

    // Синтез с окном и overlap-add (трюк выше дает N*ifft,
    // деление на N уходит сюда)
    for(int i=0; i<frameSize; ++i)
        olaBuf[i]+=fftRe[i]*sqrtHann[i]/frameSize;

    // Готовые сэмплы шага ок, сдвиг накопителя
    for(int i=0; i<hopSize; ++i)
        outQ.push_back(olaBuf[i]);

    std::memmove(olaBuf.data(), olaBuf.data()+hopSize,
                 sizeof(double)*hopSize);

    for(int i=hopSize; i<frameSize; ++i)
        olaBuf[i]=0.0;

    // Сэмплы кадра потрачены, оставляем хвост окна
    const int curSize=static_cast<int>(inRing.size());
    std::memmove(inRing.data(), inRing.data()+hopSize,
                 sizeof(double)*(curSize-hopSize));
    inRing.resize(curSize-hopSize);

    // Засев считается кадрами, а не бинами
    if(seedLeft>0)
        --seedLeft;
}
