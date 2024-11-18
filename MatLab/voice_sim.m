filename = 'voice.mp3';
N = 1024;

[voice, fs] = audioread(filename);

player = audioplayer(voice, fs);
play(player);

figure;
h = plot(nan, nan);
title('Спектр голоса в реальном времени');
xlabel('Частота (Гц)');
ylabel('Амплитуда');
grid on;


frame_size = N;
num_frames = floor(length(voice) / frame_size);

for i = 1:num_frames
    if ~isplaying(player)
        break;
    end
    
    frame = voice((i-1)*frame_size+1:i*frame_size);
    
    % Преобразование Фурье для текущего блока
    Y = fft(frame, N);
    Y = abs(Y(1:N/2+1)); % Берем только положительные частоты
    
    freqAxis = (0:N/2) * fs / N;
    set(h, 'XData', freqAxis, 'YData', Y);
    drawnow;
    
    % Пауза для синхронизации обновлений графика с воспроизведением
    pause(frame_size / fs);
end
