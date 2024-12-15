#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <xserial.hpp>
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

#define SAMPLE_RATE 24000
#define BUFFER_SIZE 8192
#define DELAY_SECONDS 2

std::atomic<bool> stopFlag(false);

class CircularBuffer
{
private:
    std::vector<std::vector<short>> buffer;
    size_t capacity;
    size_t writeIndex = 0;
    size_t readIndex = 0;
    size_t count = 0;
    std::mutex mtx;
    std::condition_variable cv;

public:
    CircularBuffer(size_t capacity) : capacity(capacity)
    {
        buffer.resize(capacity, std::vector<short>(BUFFER_SIZE));
    }

    void push(const std::vector<short> &data)
    {
        std::unique_lock<std::mutex> lock(mtx);
        buffer[writeIndex] = data;
        writeIndex = (writeIndex + 1) % capacity;
        if (count < capacity)
            ++count;
        else
            readIndex = (readIndex + 1) % capacity; // Overwrite old data
        cv.notify_one();
    }

    bool pop(std::vector<short> &data)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]
                { return count > 0 || stopFlag; });
        if (count == 0)
            return false;

        data = buffer[readIndex];
        readIndex = (readIndex + 1) % capacity;
        --count;
        return true;
    }
};

// Function to open COM port using xserial
xserial::ComPort openCOMPort(int portNumber, unsigned long baudRate)
{
    xserial::ComPort comPort;
    if (!comPort.open(portNumber, baudRate))
    {
        std::cerr << "Failed to open COM port COM" << portNumber << std::endl;
        return comPort;
    }
    std::cout << "Opened COM port COM" << portNumber << std::endl;
    return comPort;
}

void playbackAudio(CircularBuffer &buffer)
{
    HWAVEOUT hWaveOut;
    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to open audio output." << std::endl;
        return;
    }

    std::vector<short> audioData(BUFFER_SIZE);

    while (!stopFlag)
    {
        if (buffer.pop(audioData))
        {
            WAVEHDR waveHeader = {};
            waveHeader.lpData = reinterpret_cast<LPSTR>(audioData.data());
            waveHeader.dwBufferLength = audioData.size() * sizeof(short);

            if (waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
            {
                waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
                while (!(waveHeader.dwFlags & WHDR_DONE))
                {
                    Sleep(10); // Wait until playback is complete
                }
                waveOutUnprepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
            }
        }
    }

    waveOutClose(hWaveOut);
}

void receiveFromCOMPort(xserial::ComPort &comPort, CircularBuffer &circBuffer)
{
    std::vector<short> buffer(BUFFER_SIZE);

    while (!stopFlag)
    {
        unsigned long bytesRead = comPort.read(reinterpret_cast<char *>(buffer.data()), buffer.size() * sizeof(short));
        if (bytesRead > 0)
        {
            buffer.resize(bytesRead / sizeof(short));
            circBuffer.push(buffer);
        }
    }
}

void recordAudio(CircularBuffer &buffer)
{
    HWAVEIN hWaveIn;
    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = SAMPLE_RATE;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to open audio recording." << std::endl;
        return;
    }

    WAVEHDR waveHeader = {};
    std::vector<short> audioData(BUFFER_SIZE);
    waveHeader.lpData = reinterpret_cast<LPSTR>(audioData.data());
    waveHeader.dwBufferLength = BUFFER_SIZE * sizeof(short);

    if (waveInPrepareHeader(hWaveIn, &waveHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        std::cerr << "Failed to prepare header for recording." << std::endl;
        waveInClose(hWaveIn);
        return;
    }

    waveInStart(hWaveIn);

    while (!stopFlag)
    {
        if (waveInAddBuffer(hWaveIn, &waveHeader, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
        {
            buffer.push(audioData);
            while (!(waveHeader.dwFlags & WHDR_DONE))
            {
                Sleep(10);
            }
        }
    }

    waveInStop(hWaveIn);
    waveInClose(hWaveIn);
}

void sendToCOMPort(xserial::ComPort &comPort, CircularBuffer &circBuffer)
{
    std::vector<short> buffer;

    while (!stopFlag)
    {
        if (circBuffer.pop(buffer))
        {
            comPort.write(reinterpret_cast<char *>(buffer.data()), buffer.size() * sizeof(short));
        }
    }
}

int main(int argc, char *argv[])
{
    int port;
    if (argc <= 1)
    {
        std::cout << "Usage: .\\com-manager <baudrate>" << std::endl;
        return 1;
    }
    unsigned long baudRate = std::stoul(argv[1]);
    xserial::ComPort ListOfPorts;
    std::vector<std::string> ComPorts;
    ListOfPorts.getListSerialPorts(std::ref(ComPorts));
    for (size_t i = 0; i < ComPorts.size(); i++)
    {
        std::cout << ComPorts[i] << std::endl;
    }
    std::cout << "Select COM port (e.g., 1 for COM1): ";
    std::cin >> port;
    ListOfPorts.close();

    xserial::ComPort comPort = openCOMPort(port, baudRate);
    if (!comPort.getStateComPort())
    {
        return 1;
    }

    CircularBuffer recordBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);
    CircularBuffer playbackBuffer(DELAY_SECONDS * SAMPLE_RATE / BUFFER_SIZE);

    std::thread recordThread(recordAudio, std::ref(recordBuffer));
    std::thread sendThread(sendToCOMPort, std::ref(comPort), std::ref(recordBuffer));
    std::thread receiveThread(receiveFromCOMPort, std::ref(comPort), std::ref(playbackBuffer));
    std::thread playbackThread(playbackAudio, std::ref(playbackBuffer));

    std::cin.get();
    std::cin.get();

    stopFlag = true;

    recordThread.join();
    sendThread.join();
    receiveThread.join();
    playbackThread.join();

    comPort.close();

    return 0;
}
