#pragma once
#include <stdint.h>
#include <vector>
#include <thread>
#include <string>
#include "boost/thread/thread.hpp"
#include "boost/chrono/chrono.hpp"
#include "boost/container/vector.hpp"
#include <QtWidgets>
//Светофор
class semaphor : public QWidget
{
	Q_OBJECT
	private:
		enum {
			RED = 1,
			YELLOW = 2,
			GREEN = 4
		};
		std::string myName;
		uint16_t myId;	//Ид светофора
		uint8_t status;	//Регистр состояния
		uint8_t cycle_timer;
		boost::container::vector<uint8_t> neighbout_reg_state;	//Хранилище состояний соседей
		boost::container::vector<uint16_t>* neighbour;	//Контейнер для хранения ИД соседей(если таковы есть)
		boost::container::vector<std::pair<uint8_t, uint8_t>>* polling_graph;
		uint16_t waitingTimer;	//Таймер ожидания в циклах, задаётся менеджером
		uint16_t roadscanTimer;	//Таймер съемки дороги перед собой
		uint16_t transitionAllowTimer;	//Таймер разрешения проезда
		boost::thread *sThread;
		bool roadBusy;	//Флаг занятой дороги
	public:
		bool stop_thread;

		volatile uint16_t cycle_speed;
		volatile uint16_t manager_query_code;	//Код запроса от менеджера
		volatile uint16_t queue;	//Очередь из автомобилей
		volatile bool transitAllowed;	//Флаг, разрешающий переключение светофора в зеный
		semaphor();
		~semaphor();
		void run_thread();
		void new_thread(uint16_t);
		void test_hello();

		//Импорт соседа
		void add_neighbour(uint16_t n_id);

		//Для менеджера. Запрос состояния светофора
		uint8_t get_reg_state();

		//Запрос состояний соседей
		void request_neighbour_status();

		//Проверяем что у соседей(если хоть один зеленый то не едем)
		bool road_safety_check();

		//Пропуск автомобиля
		void car_pass();

		//Установка имени светофора
		void set_name(std::string name);

		//Читаем команду от менеджера
		void read_manager_command();

		//Загружаем график частоты отправления запросов 
		void set_polling_graph(std::pair<uint8_t, uint8_t>);

		//Рассчитываем частоту опроса менеджера
		void calc_query_cycle();

		//Получаем значения таймера циклов
		uint8_t get_cycle_timer_value();

	signals:
		//Сигнал для передачи информации о светофоре
		void signal_send_info(std::pair<uint16_t, uint16_t>);
};
