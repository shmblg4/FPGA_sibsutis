n = 8;      % Разрядность
N = 1e3;   % Кол-во отсчетов сигнала
fs = 24e3;  % Частота дискредитации 24 кГц

%% Создадим функцию и выведем ее график
t = 0:1/fs:1;
F = @(t) sin(2 * pi * 480 * t) + 0.5 * sin(2 * pi * 1800 * t);
S = F(t);
subplot(2, 1, 1);
plot(t, S, 'Color', 'red'); hold on;
grid on;
xlabel('Time')
ylabel('Amplitude')
ylim([-2, 2]);
xlim([0, 0.01]);

%% Возьмем n отчетов сигнала и пронормируем их
S_max = max(S);             % Максимальное значение функции S
max_signed_value = 2^(n-1); % Максимальное знаковое значение при разр. n

S_norm = zeros(1, N);
for i = 1:N
    S_norm(i) = S(i) / S_max * max_signed_value;
end

subplot(2, 1, 2);
plot(1:N, S_norm, 'Color', 'red'); hold off;
grid on;
xlabel('Steps')
ylabel('Norm. Amplitude')

%% Преобразование в доп код
max_unsigned_value = 2^n - 1; % Максимальное беззнаковое значение
S_comp_rounded = zeros(1, N);

for i = 1:N
    if S_norm(i) < 0
        S_comp_rounded(i) = round(max_unsigned_value + S_norm(i)); % Для отрицательных чисел
    else
        S_comp_rounded(i) = round(S_norm(i)); % Для положительных чисел
    end
end

% Обратное преобразование
S_reconstructed = zeros(1, N);

for i = 1:N
    if S_comp_rounded(i) > max_unsigned_value / 2
        S_reconstructed(i) = S_comp_rounded(i) - 2^n; % Обратное преобразование для отрицательных чисел
    else
        S_reconstructed(i) = S_comp_rounded(i); % Для положительных чисел
    end
end

%% Преобразование отчетов в доп код

M_bin = zeros(N, n); % Инициализация массива для двоичных представлений

for i = 1:N
    M_bin(i, :) = dec2bin(S_comp_rounded(i), n) - '0';
end

% Преобразуем матрицу в один вектор
flat_vector = reshape(M_bin', 1, []); % Транспонируем и расправляем

% Сохранение в бинарный файл
file = fopen('samples_binary.dat', 'wb'); % Открываем файл для записи в бинарном режиме
fwrite(file, flat_vector, 'uint8'); % Записываем в файл
fclose(file); % Закрываем файл

%% Чтение из файла
% Открываем файл для чтения
file = fopen('samples_binary.dat', 'rb');
if file == -1
    error('Не удалось открыть файл для чтения');
end

% Читаем содержимое файла
read_vector = fread(file, 'uint8'); % Читаем все данные
fclose(file); % Закрываем файл

% Преобразуем в матрицу обратно (если нужно)
read_matrix = reshape(read_vector, 8, [])'; % Преобразуем в матрицу 1000x8

% Преобразование строк матрицы в десятичные значения
decimal_values = bi2de(read_matrix, 'left-msb'); % Преобразуем в десятичные значения

% Проверяем результаты
disp('Первые 10 десятичных значений:');
disp(decimal_values(1:10));

% Построение графика
figure;
plot(1:1000, decimal_values, 'b-'); % Строим график
xlabel('Индексы');
ylabel('Десятичные значения');
title('График десятичных значений из двоичной матрицы');
grid on;
