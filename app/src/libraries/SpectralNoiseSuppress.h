// Подавление фонового шума голосовой записи методом спектрального
// вычитания. Сигнал нарезается перекрывающимися окнами 512 сэмплов
// (шаг 256), в спектре каждого окна величина бинов ужимается на
// оценку пола шума, сигнал собирается обратно (overlap-add).
// Пол шума - минимум сглаженной огибающей каждого бина за
// последнее время (~1 с): фон глушится и в паузах, и под речью,
// при этом короткие всплески речи пол не подтягивают. Поток
// идет чанками любого размера, на выходе ровно столько же
// сэмплов (задержка ~10 мс на старте выдается нулями)
#ifndef SPECTRALNOISESUPPRESS_H
#define SPECTRALNOISESUPPRESS_H

#include <deque>
#include <vector>

class SpectralNoiseSuppress
{
public:

    SpectralNoiseSuppress(void);
    ~SpectralNoiseSuppress(void);

    // Расчет окон и таблиц преобразования под частоту записи,
    // сброс оценок шума и буферов потока
    void setup(int sampleRate);

    // Обработка чанка: на вход len сэмплов, на выходе столько же
    void process(short *pcm, int samples);

private:

    // Прогон одного окна: окно, FFT, вычитание, IFFT, overlap-add
    void processFrame(void);

    int sampleRate;
    int frameSize;  // размер окна преобразования
    int hopSize;    // шаг окон, половина окна

    // Таблицы преобразования и окна анализа/синтеза
    std::vector<double> sqrtHann;   // корень из окна Ханна
    std::vector<double> cosTable;
    std::vector<double> sinTable;
    std::vector<int> bitRev;        // перестановка бит-реверса
    std::vector<double> fftRe;
    std::vector<double> fftIm;

    // Пол шума: сглаженная огибающая бина, кольцо истории
    // и вычисленный на кадр пол; prevGain - гейн прошлого кадра
    // (сглаживание, спад не быстрее -8 дБ за кадр). Начальный
    // засев: первые кадры реплицируются по всему кольцу, чтобы
    // подавление работало с самого старта записи
    int noiseWin;
    int ringPos;
    int seedLeft;
    std::vector<float> smoothMag;
    std::vector<float> noiseRing;
    std::vector<double> noiseFloor;
    std::vector<double> prevGain;

    // Поток: вход-накопитель кадров, очередь готовых сэмплов
    // и накопитель перекрытий
    std::vector<double> inRing;
    std::deque<double> outQ;
    std::vector<double> olaBuf;
};

#endif // SPECTRALNOISESUPPRESS_H
