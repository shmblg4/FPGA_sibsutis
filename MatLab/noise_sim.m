fs = 44100;
duration = 5;
N = 1024;

voice = randn(fs * duration, 1);  % генерация случайного сигнала

player = audioplayer(voice, fs);
play(player);

% Настройка окна для графика
figure;
h = plot(nan, nan);
title('Спектр шума в реальном времени');
xlabel('Частота (Гц)');
ylabel('Амплитуда');
grid on;

% Основной цикл для моделирования реального времени
frame_size = N;
num_frames = floor(length(voice) / frame_size);

for i = 1:num_frames
    frame = voice((i-1)*frame_size+1:i*frame_size);
    
    Y = fft(frame, N);
    Y = abs(Y(1:N/2+1)); % Берем только положительные частоты
    
    freqAxis = (0:N/2) * fs / N;
    set(h, 'XData', freqAxis, 'YData', Y);
    drawnow;
    
    pause(frame_size / fs);
end
