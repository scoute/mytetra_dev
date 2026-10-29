#include <QFile>
#include <QDateTime>
#include <QtEndian>

#include <opus.h>

#include "OpusFileWriter.h"


// Фиксированный профиль голоса: CBR 24 кбит, кадр 20 мс
static const int OPUS_VOICE_BITRATE=24000;
static const int OPUS_VOICE_FRAME_MS=20;


OpusFileWriter::OpusFileWriter(void)
{
    file=nullptr;
    encoder=nullptr;
    streamState=nullptr;
    pendingCount=0;
    inputRate=48000;
    frameSize=960;
    preskip=0;
    totalFrames=0;
    nextPacketNo=2;
    headersWritten=false;
    eosWritten=false;

    // Иначе после нескольких дублей в writePcm монтируется пакет
    // с мусорной длиной из прошлого прохода стека: segfault
    // в ogg_stream_packetin
    heldLen=0;
    heldValid=false;
}


OpusFileWriter::~OpusFileWriter(void)
{
    close();
}


bool OpusFileWriter::open(const QString &filePath, int inputSampleRate)
{
    close();

    // Энкодер понимает только эти частоты, остальное отбрасывается вызывателем
    if(inputSampleRate!=8000 && inputSampleRate!=12000 &&
       inputSampleRate!=16000 && inputSampleRate!=24000 &&
       inputSampleRate!=48000)
    {
        errorText=QStringLiteral("Unsupported sample rate");
        return false;
    }

    int error=OPUS_OK;
    encoder=opus_encoder_create(inputSampleRate, 1, OPUS_APPLICATION_VOIP, &error);

    if(error!=OPUS_OK || encoder==nullptr)
    {
        errorText=QStringLiteral("Can not create encoder");
        encoder=nullptr;
        return false;
    }

    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_VOICE_BITRATE));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(0));

    // Задержка энкодера пишется в OpusHead, плеер обрежет ее при чтении
    int lookahead=0;
    opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&lookahead));
    preskip=lookahead;

    inputRate=inputSampleRate;
    frameSize=inputSampleRate*OPUS_VOICE_FRAME_MS/1000;
    outputFrames=0;

    file=new QFile(filePath);

    if(!file->open(QIODevice::WriteOnly))
    {
        errorText=file->errorString();
        delete file;
        file=nullptr;
        opus_encoder_destroy(encoder);
        encoder=nullptr;
        return false;
    }

    streamState=new ogg_stream_state;

    // Серийник потока: время плюс адрес, коллизии в разных файлах безвредны
    const quint32 serial=static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff) ^
                         static_cast<quint32>(reinterpret_cast<quintptr>(this));

    if(ogg_stream_init(streamState, serial)!=0)
    {
        errorText=QStringLiteral("Can not init Ogg stream");
        delete streamState;
        streamState=nullptr;
        file->close();
        delete file;
        file=nullptr;
        opus_encoder_destroy(encoder);
        encoder=nullptr;
        return false;
    }

    pendingCount=0;
    totalFrames=0;
    outputFrames=0;
    headersWritten=false;
    eosWritten=false;
    nextPacketNo=2;

    // «Удержанный» кадр прошлой записи сюда не попадает: если
    // heldValid остался истинным от предыдущего дубля (например,
    // после ошибки кодирования), writePcm на первом кадре пошлет
    // пакет с чужой длиной
    heldLen=0;
    heldValid=false;

    writeHeaders();

    return true;
}


void OpusFileWriter::writeHeaders(void)
{
    // OpusHead, строго 19 байт для моно (RFC 7845, раздел 5.1)
    unsigned char head[19];
    memcpy(head, "OpusHead", 8);
    head[8]=1;
    head[9]=1;
    qToLittleEndian<quint16>(static_cast<quint16>(preskip), head+10);
    qToLittleEndian<quint32>(static_cast<quint32>(inputRate), head+12);
    qToLittleEndian<qint16>(0, head+16);
    head[18]=0;

    ogg_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.packet=head;
    packet.bytes=sizeof(head);
    packet.b_o_s=1;
    packet.granulepos=0;
    packet.packetno=0;
    ogg_stream_packetin(streamState, &packet);

    // OpusTags: вендор плюс один комментарий (RFC 7845, раздел 5.2).
    // Длины сразу в little-endian: файл обязан читаться везде
    // независимо от порядка байт хоста
    const QByteArray vendor("MyTetra");
    const QByteArray comment("ENCODER=MyTetra");
    QByteArray tags;
    tags.append("OpusTags", 8);
    tags.resize(tags.size()+4);
    qToLittleEndian<quint32>(static_cast<quint32>(vendor.size()),
                             reinterpret_cast<unsigned char *>(tags.data())+tags.size()-4);
    tags.append(vendor);
    tags.resize(tags.size()+4);
    qToLittleEndian<quint32>(1, reinterpret_cast<unsigned char *>(tags.data())+tags.size()-4);
    tags.resize(tags.size()+4);
    qToLittleEndian<quint32>(static_cast<quint32>(comment.size()),
                             reinterpret_cast<unsigned char *>(tags.data())+tags.size()-4);
    tags.append(comment);

    memset(&packet, 0, sizeof(packet));
    packet.packet=reinterpret_cast<unsigned char *>(tags.data());
    packet.bytes=tags.size();
    packet.b_o_s=0;
    packet.granulepos=0;
    packet.packetno=1;
    ogg_stream_packetin(streamState, &packet);

    flushPages(false);

    headersWritten=true;
}


bool OpusFileWriter::flushPages(bool force)
{
    ogg_page page;

    while(true)
    {
        int result=force ? ogg_stream_flush(streamState, &page)
                         : ogg_stream_pageout(streamState, &page);

        if(result==0)
            break;

        if(file->write(reinterpret_cast<const char *>(page.header), page.header_len)!=page.header_len ||
           file->write(reinterpret_cast<const char *>(page.body), page.body_len)!=page.body_len)
        {
            errorText=file->errorString();
            return false;
        }
    }

    return true;
}


// Отправка готового кадра в поток. EOS ставится только на реальный
// аудиокадр: пустой финальный пакет декодеры разворачивают в PLC-шум
bool OpusFileWriter::submitFrame(const unsigned char *data, int len, bool eos)
{
    totalFrames+=frameSize;
    outputFrames+=static_cast<long long>(frameSize)*48000/inputRate;

    ogg_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.packet=const_cast<unsigned char *>(data);
    packet.bytes=len;
    // Granulepos всегда в единицах 48 кГц, независимо от входа
    packet.granulepos=outputFrames+preskip;
    packet.packetno=nextPacketNo++;

    if(eos)
        packet.e_o_s=1;

    ogg_stream_packetin(streamState, &packet);

    return flushPages(false);
}


bool OpusFileWriter::writePcm(const short *pcm, int sampleCount)
{
    if(file==nullptr || encoder==nullptr)
        return false;

    int offset=0;

    while(offset<sampleCount)
    {
        // Добивка внутреннего буфера до целого кадра
        const int need=frameSize-pendingCount;
        const int take=qMin(need, sampleCount-offset);

        memcpy(pending+pendingCount, pcm+offset, static_cast<size_t>(take)*sizeof(short));
        pendingCount+=take;
        offset+=take;

        if(pendingCount<frameSize)
            break;

        unsigned char encoded[4000];
        const opus_int32 encodedBytes=opus_encode(encoder, pending, frameSize, encoded, sizeof(encoded));

        if(encodedBytes<0)
        {
            errorText=QStringLiteral("Encode error");
            return false;
        }

        // Последний готовый кадр придерживается: EOS должен стоять
        // на реальном аудио, а не на пустом пакете. heldLen
        // ограничен размером heldData: даже если состояние
        // испорчено извне, пакет в ogg не уйдет с дурной длиной
        if(heldValid && heldLen>0 && heldLen<=static_cast<int>(sizeof(heldData)))
        {
            if(!submitFrame(heldData, heldLen, false))
                return false;
        }

        memcpy(heldData, encoded, static_cast<size_t>(encodedBytes));
        heldLen=encodedBytes;
        heldValid=true;

        pendingCount=0;
    }

    return true;
}


qint64 OpusFileWriter::durationMs(void) const
{
    return static_cast<qint64>(outputFrames)/48;
}


bool OpusFileWriter::close(void)
{
    if(file==nullptr)
        return true;

    bool ok=true;

    // Добивка остатка нулями до ближайшего допустимого размера
    // (2.5/5/10/20 мс). EOS всегда на реальном аудио, не на пустом пакете
    if(pendingCount>0 && encoder!=nullptr)
    {
        static const int validFrames[]={120, 240, 480, 960};

        int frame=960;

        for(int candidate : validFrames)
            if(candidate>=pendingCount)
            {
                frame=candidate;
                break;
            }

        memset(pending+pendingCount, 0, static_cast<size_t>(frame-pendingCount)*sizeof(short));

        unsigned char encoded[4000];
        const opus_int32 encodedBytes=opus_encode(encoder, pending, frame, encoded, sizeof(encoded));

        if(encodedBytes>=0)
        {
            if(heldValid && heldLen>0 && heldLen<=static_cast<int>(sizeof(heldData)))
            {
                if(!submitFrame(heldData, heldLen, false))
                    ok=false;

                heldValid=false;
            }

            totalFrames+=frame;
            outputFrames+=static_cast<long long>(frame)*48000/inputRate;

            ogg_packet packet;
            memset(&packet, 0, sizeof(packet));
            packet.packet=encoded;
            packet.bytes=encodedBytes;
            packet.granulepos=outputFrames+preskip;
            packet.packetno=nextPacketNo++;
            packet.e_o_s=1;
            ogg_stream_packetin(streamState, &packet);
        }
        else
        {
            ok=false;
            heldValid=false;
            heldLen=0;
        }

        pendingCount=0;
    }
    else if(heldValid && heldLen>0 && heldLen<=static_cast<int>(sizeof(heldData)))
    {
        if(!submitFrame(heldData, heldLen, true))
            ok=false;

        heldValid=false;
    }
    else
    {
        // Ни остатка, ни корректного удержанного кадра: EOS был
        // отдан бэкендом явной ошибкой в прошлой попытке, и не
        // надо уносить это состояние в следующий дубль
        heldValid=false;
        heldLen=0;
    }

    if(!flushPages(true))
        ok=false;

    file->close();
    delete file;
    file=nullptr;

    if(encoder!=nullptr)
    {
        opus_encoder_destroy(encoder);
        encoder=nullptr;
    }

    if(streamState!=nullptr)
    {
        ogg_stream_clear(streamState);
        delete streamState;
        streamState=nullptr;
    }

    if(!ok && errorText.isEmpty())
        errorText=QStringLiteral("Write error");

    return ok;
}


bool OpusFileWriter::isOpen(void) const
{
    return file!=nullptr;
}


QString OpusFileWriter::errorString(void) const
{
    return errorText;
}
