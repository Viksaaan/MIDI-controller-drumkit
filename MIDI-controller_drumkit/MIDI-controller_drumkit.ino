// управление жидкокристаллическими экранами
#include <LiquidCrystal_I2C.h>

// чтение и запись в постоянное хранилище
#include <EEPROM.h>

// обмен данными с устройствами I2C/TWI
#include <Wire.h>

// количество дрампадов
#define PADS 9

// скорость работы монитора порта
#define BAUD_RATE 115200

// указываем [I2C-адрес] и размерность: [кол-во столбцов], [кол-во строк]
LiquidCrystal_I2C lcd(0x27, 16, 2);


// указывает, применено ли редактирование
//  1 - true - Применено
//  2 - false - НЕ применено
bool confirm_edit = true;

// указывает, активен ли режим редактирования
//  1 - true - Активен
//  2 - false - НЕ активен
bool mode_edit_on = false;

// отражает статус режима редактирования на LCD-дисплее:
//  1 - ["(edit)"] - режим редактирования Активен
//  2 - [""] - режим редактирования НЕ активен
String status = "";


// выбор инструмента и установка значений настройки
byte INC_DEC = 0;

// выбор настройки
byte NEXT_BACK = 0;

// значение пьезодатчика
short volume = 0;

// настройка дрампадов (частоты нот)
// на основе плагина Addictive Drums (FL Studio)
byte note[PADS] = {
	65, // A0 – T4
	36, // A1 - Kick
	38, // A2 - Snare
	69, // A3 – T2
	71, // A4 – T1
	46, // A5 – C1
	45, // A6 - Ride
	79, // A7 – C2
	51	// A8 - HH
};


// нижний порог срабатывания
short min_limit[PADS] = {
	100, // A0 – T4
	100, // A1 - Kick
	100, // A2 - Snare
	100, // A3 – T2
	100, // A4 – T1
	100, // A5 – C1
	300, // A6 - Ride
	300, // A7 – C2
	300	 // A8 - HH
};


// частота опроса (длительность игнорирования) датчиков (мс)
short scan_time[PADS] = {
	20, // A0 – T4
	20, // A1 - Kick
	20, // A2 - Snare
	20, // A3 – T2
	20, // A4 – T1
	20, // A5 – C1
	20, // A6 - Ride
	20, // A7 – C2
	20	// A8 - HH
};


// чувствительность
short sensitivity[PADS] = {
	80, // A0 – T4
	80, // A1 - Kick
	80, // A2 - Snare
	80, // A3 – T2
	80, // A4 – T1
	80, // A5 – C1
	80, // A6 - Ride
	80, // A7 – C2
	80	// A8 - HH
};


// состояние проигрывания инструмента
boolean playing[PADS] = {
	false, // A0 – T4
	false, // A1 - Kick
	false, // A2 - Snare
	false, // A3 – T2
	false, // A4 – T1
	false, // A5 – C1
	false, // A6 - Ride
	false, // A7 – C2
	false  // A8 - HH
};


// громкость (регулируется методом playNote())
short high_score[PADS] = {
	0, // A0 – T4
	0, // A1 - Kick
	0, // A2 - Snare
	0, // A3 – T2
	0, // A4 – T1
	0, // A5 – C1
	0, // A6 - Ride
	0, // A7 – C2
	0  // A8 - HH
};


// массив времени воспроизведения каждого инструмента
// хранит время последнего воспроизведения инструмента
unsigned long timer[PADS] = {
	0, // A0 – T4
	0, // A1 - Kick
	0, // A2 - Snare
	0, // A3 – T2
	0, // A4 – T1
	0, // A5 – C1
	0, // A6 - Ride
	0, // A7 – C2
	0  // A8 - HH
};


/*  шаг изменения настройки
				-Note (0..127)
				|  -Sensitivity (50..100)
				|  |   -Min limit (20..1000)
				|  |   |  -Scan time (10..1000)
				|  |   |  | */
byte step[4] = {1, 1, 10, 5};


// набор инструментов для настройки
char *instrument[] = {
	"Tom 3",
	"Kick",
	"Snare",
	"Tom 2",
	"Tom 1",
	"Cymbal 1",
	"Ride",
	"Cymbal 2",
	"HiHat"
};


// набор настроек для инструментов (отображаются на lcd 16x2,
// поэтому учитывать длину названий / показаний параметров)
char *setting[] = {
	"Note: ",
	"Sensitivity:",
	"Min limit: ",
	"Scan time: "
};


// начальные номера ячеек для хранения настроек в EEPROM
byte addr_note = 0;			 // 0-8   - note         (1 byte) 
short addr_min_limit = 10;	 // 10-27 - min_limit    (2 byte) x (9 pads)
short addr_scan_time = 30;	 // 30-47 - scan_time    (2 byte) x (9 pads)
short addr_sensitivity = 50; // 50-67 - sensitivity  (2 byte) x (9 pads)

// объявление переменных для кнопок управления
short button_EDIT;
short button_INC;
short button_DEC;
short button_NEXT;
short button_BACK;


/* 	Метод выравнивает силу звучания
	(влияет на громкость от силы удара)
	param [pad] - пьезодатчик
	param [volume] - данные пьезодатчика
*/
void playNote(byte pad, short volume)
{
	float velocity = (volume / (float)sensitivity[pad]) * 10;

	// корректировка при выходе за пределы громкости
	if (velocity > 127)
		velocity = 127;

	// выравнивание силы звучания
	if (velocity > high_score[pad])
		high_score[pad] = velocity;
}


/*	Метод активирует звучание ноты
 *	param [channel] - канал 
 *	param [note] - нота
 *	param [velocity] - громкость
 */
void noteOn(byte channel, byte note, short velocity){

	// отправка MIDI-сообщений
	Serial.write(channel);
	Serial.write(note);
	Serial.write(velocity);
}


/* 	Метод прекращает звучание ноты (за счет нулевой громкости)
 *	param [channel] - канал
 *	param [note] - нота
 */
void noteOff(byte channel, byte note){

	// отправка MIDI-сообщений
	Serial.write(channel);
	Serial.write(note);
	Serial.write(0);
}


/* 	[Внимание] - Метод записывает данные в EEPROM (производится единожды)
  	позволяет использовать устройство без перепрошивки,
  	используя т.н. "заводские" настройки
*/
void set_EEPROM(){
	for (byte i = 0; i < PADS; i++)
	{
		// помещаем в EEPROM значение настроек 
		EEPROM.put(addr_note, note[i]);
		EEPROM.put(addr_sensitivity, sensitivity[i]);
		EEPROM.put(addr_min_limit, min_limit[i]);
		EEPROM.put(addr_scan_time, scan_time[i]);

		// назначаем адрес для следующей записи.
		// [Внимание] В целях экономии памяти в качестве адреса
		// 	    	  используются глобальные переменные
		addr_note += sizeof(addr_note);
		addr_sensitivity += sizeof(sensitivity);
		addr_min_limit += sizeof(addr_min_limit);
		addr_scan_time += sizeof(addr_scan_time);
	}

	// [Внимание] после записи возвращаем начальные номера ячеек
	addr_note = 0;			 // 0-8   - note         (1 byte)
	addr_min_limit = 10;	 // 10-27 - min_limit    (2 byte) x (9 pads)
	addr_scan_time = 30;	 // 30-47 - scan_time    (2 byte) x (9 pads)
	addr_sensitivity = 50;   // 50-67 - sensitivity  (2 byte) x (9 pads)
}


// Метод считывает данные из EEPROM
void get_EEPROM(){

	// [Внимание] возвращаем начальные номера ячеек хранения настроек в EEPROM
	// 			  для последовательного считывания значений
	addr_note = 0;			 // 0-8   - note         (1 byte)
	addr_min_limit = 10;	 // 10-27 - min_limit    (2 byte) x (9 pads)
	addr_scan_time = 30;	 // 30-47 - scan_time    (2 byte) x (9 pads)
	addr_sensitivity = 50;   // 50-67 - sensitivity  (2 byte) x (9 pads)

	for (byte i = 0; i < PADS; i++)
	{
		// считываем из EEPROM значение настроек 
		EEPROM.get(addr_note, note[i]);
		EEPROM.get(addr_sensitivity, sensitivity[i]);
		EEPROM.get(addr_min_limit, min_limit[i]);
		EEPROM.get(addr_scan_time, scan_time[i]);

		// определяем следующий адрес для чтения 
		addr_note += sizeof(addr_note);
		addr_sensitivity += sizeof(sensitivity);
		addr_min_limit += sizeof(addr_min_limit);
		addr_scan_time += sizeof(addr_scan_time);
	}
}


// [Внимание] - Метод очищает память EEPROM
void cls_EEPROM(){
	for (byte mem_cell = 0; mem_cell < 100; mem_cell++)
		EEPROM.update(mem_cell, 0);
}


/* 	Метод отображает на LCD-дисплее данные 
	проигрываемого инструмента
*/
void print_play_info(byte i, short volume)
{
	// очистка дисплея
	lcd.clear();

	// вывод в первой строке дисплея проигрываемого инструмента
	lcd.print(instrument[i]);

	// установка курсора дисплея в начало второй строки
	lcd.setCursor(0, 1);

	// вывод во второй строке дисплея значения проигрываемого инструмента
	lcd.print(volume);
}


/**********************   S E T U P   **************************/

/* 	В данном методе происходит установка и инициализация 
	  начальных значений параметров устройства
*/
void setup()
{
	Serial.begin(BAUD_RATE);

    // инициализация LCD-дисплея
    lcd.init();

    // включение подсветки
    lcd.setBacklight(true);

    // текст приветствия на дисплее при запуске
    lcd.print("welcome!");
    lcd.setCursor(0, 1);
    lcd.print("drum kit ready");

	// Установка режима работы кнопок
	for (byte b = 8; b <= 12; b++){

		/*
			конфигурируем как ВХОД и задействуем внутренний подтягивающий резистор.
			Использование подтягивающего резистора означает,
			что логика кнопки инвертирована. Это значит, что будет HIGH,
			когда кнопка НЕ нажата, и LOW, когда она нажата
		*/
		pinMode(b, INPUT_PULLUP);
	}


	// [Внимание] - Очистка памяти EEPROM
	// После выполнения закомментировать.
	// cls_EEPROM();


	// [Внимание] - Запись данных в EEPROM
	// Выполнять при первом запуске или при перезаписи.
	// После выполнения закомментировать.
	// set_EEPROM();

	// Чтение данных из EEPROM
	//get_EEPROM();
}


/***********************   L O O P   ***************************/

// данный метод будет выполняться до тех пор,
// пока устройство будет включено
// (подключено к источнику питания)
void loop()
{
	// инициализация кнопок управления
	button_EDIT = digitalRead(12);
	button_INC = digitalRead(9);
	button_DEC = digitalRead(8);
	button_NEXT = digitalRead(11);
	button_BACK = digitalRead(10);


	// считываем данные с пьезоэлементов
	for (byte i = 0; i < PADS; i++){

		volume = analogRead(i);

		// если считанное значение:
		//  1 - не меньше минимального предела
		//  2 - данный пьезодатчик в состоянии [НЕ проигрывается]
		if (volume >= min_limit[i] && playing[i] == false){

			// если частота опроса датчика позволяет его обрабатывать
			if (millis() - timer[i] >= scan_time[i]){

				// выравниваем звучание (high_score)
				playNote(i, volume);

				// активация звучания ноты
				noteOn(0x91, note[i], high_score[i]);
        
				// прекращение звучания ноты
				noteOff(0x91, note[i]);

				// отмечаем данный пьезодатчик
				// как в состоянии [проигрывается]
				playing[i] = true;

				// отображаем на LCD-дисплее данные проигрываемого инструмента
				print_play_info(i, volume);
			}
		}

		// иначе если считанное значение:
		// 1 - не меньше минимального предела
		// 2 - данный пьезодатчик в состоянии [проигрывается]
		else if (volume >= min_limit[i] && playing[i] == true){

			// выравниваем звучание (high_score)
			playNote(i, volume);

			// активация звучания ноты
			noteOn(0x91, note[i], high_score[i]);
      
      		// прекращение звучания ноты
			noteOff(0x91, note[i]);

			// отображение на LCD-дисплее данных проигрываемого инструмента
			print_play_info(i, volume);
		}

		// иначе если считанное значение:
		// 1 - МЕНЬШЕ минимального предела
		// 2 - данный пьезодатчик в состоянии [проигрывается]
		else if (volume < min_limit[i] && playing[i] == true){

			// прекращение звучания ноты
			noteOff(0x91, note[i]);

			// обнуляем громкость
			high_score[i] = 0;

			// переводим пьезодатчик в состоянии [НЕ проигрывается]
			playing[i] = false;

			// прошло мс с начала работы устройства
			// (обновляем последнее время проигрывания пьезодатчика)
			timer[i] = millis();
		}
	}


	// если редактирование начато:
	//  1 - нажата кнопка [EDIT]
	//  2 - параметр [применить редактирование] = [Завершено]
	//  3 - режим редактирования [НЕ активен]
	if (button_EDIT == LOW && confirm_edit == true && mode_edit_on == false) {

		// Очистка дисплея
		lcd.clear();

		// текст "EDIT" на дисплее указывает,
		// что устройство в режиме редактирования
		lcd.print("EDIT");

		// указываем, что редактирование [Не завершено]
		confirm_edit = false;

		// указываем, что режим редактирования [Активен]
		mode_edit_on = true;
		status = "(edit)";

		// задержка для отображения текста
		delay(500);
	}


	// если редактирование завершено:
	//  1 - нажата кнопка [EDIT]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [Активен]
	else if (button_EDIT == LOW && confirm_edit == true && mode_edit_on == true){

		lcd.clear();

		// текст "EDIT DONE" на дисплее указывает,
		// что редактированиe завершено
		lcd.print("EDIT DONE");

		// указываем, что редактирование [Не завершено]
		confirm_edit = false;

		// указываем, что режим редактирования [НЕ активен]
		mode_edit_on = false;
		status = "";

		delay(500);
	}


	// изменение настроек
	//  1 - нажата кнопка [INC]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [Активен]
	if (button_INC == LOW && confirm_edit == true && mode_edit_on == true){

		// [+] note
		if (NEXT_BACK == 0){

			// изменение частоты ноты (+1)
			note[INC_DEC] += step[NEXT_BACK];

			// ограничение на изменение настройки
			if (note[INC_DEC] > 127)
				note[INC_DEC] = 127;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC, note[INC_DEC]);
		}

		// [+] sensitivity
		else if (NEXT_BACK == 1){

			// изменение чувствительности (+1)
			sensitivity[INC_DEC] += step[NEXT_BACK];

			// ограничение на изменение настройки
			if (sensitivity[INC_DEC] > 100)
				sensitivity[INC_DEC] = 100;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 10, sensitivity[INC_DEC]);
		}

		// [+] min_limit
		else if (NEXT_BACK == 2){

			// изменение нижнего порога срабатывания (+10)
			min_limit[INC_DEC] += step[NEXT_BACK];

			// ограничение на изменение настройки
			if (min_limit[INC_DEC] > 1000)
				min_limit[INC_DEC] = 1000;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 30, min_limit[INC_DEC]);
		}

		// [+] scan_time
		else if (NEXT_BACK == 3){

			// изменение частоты опроса (+5)
			scan_time[INC_DEC] += step[NEXT_BACK];

			// ограничение на изменение настройки
			if (scan_time[INC_DEC] > 1000)
				scan_time[INC_DEC] = 1000;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 50, scan_time[INC_DEC]);
		}

		confirm_edit = false;
		delay(30);
	}


	// изменение настроек
	//  1 - нажата кнопка [DEC]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [Активен]
	else if (button_DEC == LOW && confirm_edit == true && mode_edit_on == true){

		// [-] note
		if (NEXT_BACK == 0){

			// ограничение на изменение настройки
			if (note[INC_DEC] - step[NEXT_BACK] < 0)
				note[INC_DEC] = 0;
			else 
				// изменение частоты ноты (-1)
				note[INC_DEC] -= step[NEXT_BACK];

			// // ограничение на изменение настройки
			// if (note[INC_DEC] < 0)
			// 	note[INC_DEC] = 0;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC, note[INC_DEC]);
		}

		// [-] sensitivity
		else if (NEXT_BACK == 1){

			// изменение чувствительности (-1)
			sensitivity[INC_DEC] -= step[NEXT_BACK];

			// ограничение на изменение настройки
			if (sensitivity[INC_DEC] < 50)
				sensitivity[INC_DEC] = 50;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 10, sensitivity[INC_DEC]);
		}

		// [-] min_limit
		else if (NEXT_BACK == 2){

			// изменение нижнего порога срабатывания (-10)
			min_limit[INC_DEC] -= step[NEXT_BACK];

			// ограничение на изменение настройки
			if (min_limit[INC_DEC] < 20)
				min_limit[INC_DEC] = 20;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 30, min_limit[INC_DEC]);
		}

		// [-] scan_time
		else if (NEXT_BACK == 3){

			// изменение частоты опроса (-5)
			scan_time[INC_DEC] -= step[NEXT_BACK];

			// ограничение на изменение настройки
			if (scan_time[INC_DEC] < 10)
				scan_time[INC_DEC] = 10;

			// обновление настройки в памяти
			EEPROM.update(INC_DEC * sizeof(short) + 50, scan_time[INC_DEC]);
		}

		confirm_edit = false;
		delay(30);
	}


	// ► INC
	// выбор инструмента
	//  1 - нажата кнопка [INC]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [НЕ активен]
	if (button_INC == LOW && confirm_edit == true && mode_edit_on == false){

		// выбор следующего инструмента
		INC_DEC = ++INC_DEC;

		// корректировка при выходе за пределы выбора
		if (INC_DEC > 8)
			INC_DEC = 0;

		confirm_edit = false;
		delay(30);
	}


	// ◄ DEC
	// выбор инструмента
	//  1 - нажата кнопка [DEC]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [НЕ активен]
	else if (button_DEC == LOW && confirm_edit == true && mode_edit_on == false){

		// корректировка при выходе за пределы выбора
		if (INC_DEC - 1 < 0)
			INC_DEC = 8;
		else
			// выбор предыдущего инструмента
			INC_DEC = --INC_DEC;

		confirm_edit = false;
		delay(30);
	}


	// ► NEXT
	// выбор настройки
	//  1 - нажата кнопка [NEXT]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [НЕ активен]
	if (button_NEXT == LOW && confirm_edit == true && mode_edit_on == false){

		// выбор следующей настройки
		NEXT_BACK = ++NEXT_BACK;

		// корректировка при выходе за пределы выбора
		if (NEXT_BACK > 3)
			NEXT_BACK = 0;

		confirm_edit = false;
		delay(30);
	}


	// ◄ BACK
	// выбор настройки
	//  1 - нажата кнопка [BACK]
	//  2 - редактирование [Применено]
	//  3 - режим редактирования [НЕ активен]
	else if (button_BACK == LOW && confirm_edit == true && mode_edit_on == false){

		// корректировка при выходе за пределы выбора
		if (NEXT_BACK - 1 < 0)
			NEXT_BACK = 3;
		else 
			// выбор предыдущей настройки
			NEXT_BACK = --NEXT_BACK;

		confirm_edit = false;
		delay(30);
	}


	// завершение редактирования
	//  1 - редактирование [Не применено]
	//  2 - ни одна из кнопок НЕ нажата
	if (confirm_edit == false && button_INC == HIGH && button_DEC == HIGH && button_NEXT == HIGH && button_BACK == HIGH && button_EDIT == HIGH){

		lcd.clear();

		// вывод инструмента на первую строку дисплея
		lcd.print(instrument[INC_DEC]);

		// вывод на первую строку дисплея статуса режима редактирования
		lcd.setCursor(10, 0);
		lcd.print(status);

		// вывод на вторую строку дисплея названия настройки
		lcd.setCursor(0, 1);
		lcd.print(setting[NEXT_BACK]);
		lcd.setCursor(13, 1);

		// вывод на вторую строку дисплея значения настройки
		// выбранного инструмента
		if (NEXT_BACK == 0)
			lcd.print(note[INC_DEC]);

		else if (NEXT_BACK == 1)
			lcd.print(sensitivity[INC_DEC]);

		else if (NEXT_BACK == 2)
			lcd.print(min_limit[INC_DEC]);

		else if (NEXT_BACK == 3)
			lcd.print(scan_time[INC_DEC]);

		// указываем, что редактирование [Завершено]
		confirm_edit = true;
	}
}