#include <QFile>
#include <QtEndian>

#include <opus.h>

#include "OpusFileReader.h"


OpusFileReader::OpusFileReader(void)
{
    file=nullptr;
    decoder=nullptr;
    streamInited=false;
    streamSerial=-1;
    channelCount=0;
    outRate=48000;
    preskip=0;
    preskipLeft=0;
    decodedFrames=0;
    totalMs=0;
    spillCount=0;

    ogg_sync_init(&syncState);
}


OpusFileReader::~OpusFileReader(void)
{
    close();
}


bool OpusFileReader::open(const QString &path, int outputRate)
{
    close();

    if(outputRate!=8000 && outputRate!=12000 && outputRate!=16000 &&
       outputRate!=24000 && outputRate!=48000)
    {
        errorText=QStringLiteral("Unsupported output rate");
        return false;
    }

    file=new QFile(path);

    if(!file->open(QIODevice::ReadOnly))
    {
        errorText=file->errorString();
        delete file;
        file=nullptr;
        return false;
    }

    filePath=path;
    outRate=outputRate;

    ogg_sync_init(&syncState);

    const int firstSkip=openStream();

    if(firstSkip<0)
    {
        close();
        return false;
    }

    preskip=firstSkip;

    // Сканирование гоняет файловый указатель и оставляет sync-буфер
    // исчерпанным. Переустановка потока с нуля: заголовки читаются заново
    totalMs=scanDuration();

    ogg_stream_clear(&streamState);
    streamInited=false;
    ogg_sync_clear(&syncState);
    ogg_sync_init(&syncState);
    file->seek(0);

    const int skip=openStream();

    if(skip<0)
    {
        close();
        return false;
    }

    preskip=skip;
    preskipLeft=skip;

    int error=OPUS_OK;
    decoder=opus_decoder_create(outRate, channelCount, &error);

    if(error!=OPUS_OK || decoder==nullptr)
    {
        errorText=QStringLiteral("Can not create decoder");
        decoder=nullptr;
        close();
        return false;
    }

    decodedFrames=0;
    spillCount=0;

    return true;
}


int OpusFileReader::openStream(void)
{
    // Первый пакет обязан быть OpusHead, второй OpusTags
    ogg_packet packet;

    if(nextPacket(packet)!=1)
    {
        errorText=QStringLiteral("Not an Ogg file");
        return -1;
    }

    if(packet.bytes<19 || memcmp(packet.packet, "OpusHead", 8)!=0)
    {
        errorText=QStringLiteral("Not an Opus file");
        return -1;
    }

    if(packet.packet[8]!=1)
    {
        errorText=QStringLiteral("Unsupported OpusHead version");
        return -1;
    }

    channelCount=packet.packet[9];

    if(channelCount<1 || channelCount>2)
    {
        errorText=QStringLiteral("Unsupported channel count");
        return -1;
    }

    // Только mapping family 0 (моно/стерео без таблицы каналов)
    if(packet.bytes<19 || packet.packet[18]!=0)
    {
        errorText=QStringLiteral("Unsupported channel mapping");
        return -1;
    }

    const int skip=packet.packet[10] | (packet.packet[11]<<8);

    if(nextPacket(packet)!=1)
    {
        errorText=QStringLiteral("Missing OpusTags");
        return -1;
    }

    if(packet.bytes<8 || memcmp(packet.packet, "OpusTags", 8)!=0)
    {
        errorText=QStringLiteral("Missing OpusTags");
        return -1;
    }

    return skip;
}


int OpusFileReader::nextPacket(ogg_packet &packet)
{
    ogg_page page;

    while(true)
    {
        // Сначала разбираем то, что уже есть в буфере sync
        while(ogg_sync_pageout(&syncState, &page)==1)
        {
            const int serial=ogg_page_serialno(&page);

            if(!streamInited)
            {
                ogg_stream_init(&streamState, serial);
                streamInited=true;
            }

            if(serial!=streamState.serialno)
                continue;

            ogg_stream_pagein(&streamState, &page);

            if(ogg_stream_packetout(&streamState, &packet)==1)
                return 1;
        }

        // Sync-буфер пуст, но в очереди стрима могли остаться пакеты
        // с ранее разобранных страниц: отдать их прежде чем сдаться
        if(streamInited && ogg_stream_packetout(&streamState, &packet)==1)
            return 1;

        if(file==nullptr || file->atEnd())
            return 0;

        char *buffer=ogg_sync_buffer(&syncState, 4096);
        const qint64 bytes=file->read(buffer, 4096);

        if(bytes<0)
        {
            errorText=file->errorString();
            return -1;
        }

        if(bytes==0)
            return 0;

        ogg_sync_wrote(&syncState, static_cast<long>(bytes));
    }
}


qint64 OpusFileReader::scanDuration(void)
{
    if(file==nullptr)
        return 0;

    const qint64 fileSize=file->size();

    if(fileSize<27)
        return 0;

    // Последняя страница целиком влезает в 64К: страница Ogg не больше ~65К
    const qint64 window=qMin<qint64>(fileSize, 65536);
    const qint64 base=fileSize-window;

    if(!file->seek(base))
        return 0;

    const QByteArray tail=file->read(window);

    if(tail.size()!=window)
        return 0;

    const char *data=tail.constData();

    // Ищем последнюю страницу с валидным granulepos, идем с конца
    for(qint64 pos=tail.size()-27; pos>=0; --pos)
    {
        if(memcmp(data+pos, "OggS", 4)!=0)
            continue;

        if(static_cast<unsigned char>(data[pos+4])!=0)
            continue;

        const int segments=static_cast<unsigned char>(data[pos+26]);

        if(pos+27+segments>tail.size())
            continue;

        // Суммируем таблицу сегментов: страница обязана лежать в буфере целиком
        qint64 bodySize=0;

        for(int seg=0; seg<segments; ++seg)
            bodySize+=static_cast<unsigned char>(data[pos+27+seg]);

        if(pos+27+segments+bodySize>tail.size())
            continue;

        qint64 granule=0;

        for(int b=0; b<8; ++b)
            granule|=(static_cast<qint64>(static_cast<unsigned char>(data[pos+6+b])))<<(b*8);

        if(granule<0)
            continue;

        const qint64 samples=granule-preskip;

        if(samples<0)
            continue;

        // Granulepos Opus всегда в единицах 48 кГц
        return samples*1000/48000;
    }

    return 0;
}


int OpusFileReader::readPcm(short *out, int maxFrames)
{
    if(file==nullptr || decoder==nullptr)
        return -1;

    // Сначала отдаем остаток прошлого кадра, не влезший в буфер вызывателя
    if(spillCount>0)
    {
        const int give=qMin(spillCount/channelCount, maxFrames);
        memcpy(out, spill, static_cast<size_t>(give*channelCount)*sizeof(short));
        spillCount-=give*channelCount;

        if(spillCount>0)
            memmove(spill, spill+give*channelCount, static_cast<size_t>(spillCount)*sizeof(short));

        decodedFrames+=give;

        return give;
    }

    ogg_packet packet;

    const int result=nextPacket(packet);

    if(result!=1)
        return result;

    // Максимальный кадр Opus 120 мс на 48 кГц: 5760 фреймов
    static short decoded[5760*2];

    const int frames=opus_decode(decoder, packet.packet, packet.bytes,
                                 decoded, 5760, 0);

    if(frames<0)
        return 0;

    int skip=0;

    if(preskipLeft>0)
    {
        skip=qMin(preskipLeft, frames*channelCount);
        preskipLeft-=skip;
    }

    const int total=frames*channelCount-skip;
    const int copy=qMin(total, maxFrames*channelCount);

    memcpy(out, decoded+skip, static_cast<size_t>(copy)*sizeof(short));

    // Невлезший остаток копится до следующего вызова, а не теряется
    spillCount=total-copy;

    if(spillCount>0)
        memcpy(spill, decoded+skip+copy, static_cast<size_t>(spillCount)*sizeof(short));

    decodedFrames+=copy/channelCount;

    return copy/channelCount;
}


bool OpusFileReader::seekMs(qint64 ms)
{
    if(file==nullptr || ms<0)
        return false;

    // Переоткрытие потока и пропуск декодированием: для минутных
    // заметок дешевле честного сика по страницам
    ogg_stream_clear(&streamState);
    streamInited=false;
    ogg_sync_clear(&syncState);
    ogg_sync_init(&syncState);
    file->seek(0);

    if(openStream()<0)
        return false;

    preskipLeft=preskip;

    int error=OPUS_OK;
    OpusDecoder *fresh=opus_decoder_create(outRate, channelCount, &error);

    if(error!=OPUS_OK || fresh==nullptr)
        return false;

    opus_decoder_destroy(decoder);
    decoder=fresh;

    decodedFrames=0;
    spillCount=0;

    const long long targetFrames=ms*outRate/1000;
    short sink[1920*2];

    while(decodedFrames<targetFrames)
    {
        const long long left=targetFrames-decodedFrames;
        const int want=static_cast<int>(qMin<long long>(left, 1920));
        const int got=readPcm(sink, want);

        if(got<=0)
            break;
    }

    return true;
}


int OpusFileReader::channels(void) const
{
    return channelCount;
}


int OpusFileReader::outputRate(void) const
{
    return outRate;
}


qint64 OpusFileReader::durationMs(void) const
{
    return totalMs;
}


qint64 OpusFileReader::positionMs(void) const
{
    if(outRate<=0)
        return 0;

    return decodedFrames*1000/outRate;
}


bool OpusFileReader::isOpen(void) const
{
    return file!=nullptr;
}


QString OpusFileReader::errorString(void) const
{
    return errorText;
}


void OpusFileReader::close(void)
{
    if(streamInited)
    {
        ogg_stream_clear(&streamState);
        streamInited=false;
    }

    ogg_sync_clear(&syncState);

    if(decoder!=nullptr)
    {
        opus_decoder_destroy(decoder);
        decoder=nullptr;
    }

    if(file!=nullptr)
    {
        file->close();
        delete file;
        file=nullptr;
    }

    decodedFrames=0;
    totalMs=0;
    preskipLeft=0;
    spillCount=0;
}
