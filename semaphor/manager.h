#pragma once
#include "semaphor.h"
#include "boost/thread/thread.hpp"
#include "boost/chrono/chrono.hpp"
#include "boost/container/vector.hpp"
#include "boost/fusion/include/pair.hpp"
#include <vector>
#include <atomic>
#include <map>
#include <QtWidgets>
class semaphor_manager;
class singlet_destroyer
{
	private:
		semaphor_manager* m_inst;
	public:
		~singlet_destroyer();
		void initialize(semaphor_manager*);
};

//Менеджер-синглтон
class semaphor_manager
{
	private:
		bool generator_mode;
		bool calc;
		std::map<uint16_t, uint16_t> *queue_list;
		std::map<uint16_t, boost::container::vector<uint16_t>> *semaphor_map;
		boost::container::vector<std::pair<uint8_t, uint8_t>>* polling_graph;
		//std::vector<std::pair<uint16_t, uint16_t>> *queue_list;
		boost::thread *generator_thread;
		boost::thread *manager_thread;
		boost::container::vector<semaphor*> s_list;
		boost::container::vector<uint16_t> s_queue;
		boost::container::vector<uint16_t> *s_id;
		boost::container::vector<uint8_t> *reg_s_state;
		boost::container::vector<boost::container::vector<uint8_t>> *zone_list;
		//std::vector<std::pair<uint8_t, uint8_t>> *transit_order;	//Порядок активации светофора UPD --> std::map
		uint16_t semaphors_cnt;
		uint16_t semaphors_map_size;
		uint16_t queue_delay;
		uint8_t n_zone;
		uint8_t zone;
		uint8_t manager_cycle;
		bool stop_thread;
		static semaphor_manager* manager_ptr;
		static singlet_destroyer dstr;
		void queueGenerator();
		void queueManager();
	protected:
		semaphor_manager();
		semaphor_manager(const semaphor_manager&);
		semaphor_manager& operator=(semaphor_manager&);
		~semaphor_manager();
		friend class singlet_destroyer;
	public:
		enum class status_codes	//fix C26812
		{
			STATUS_REQUEST = 0x01,
			TRANSIT_ALLOWED = 0x02,
			TRANSIT_DISALLOWED = 0x03,
			SET_SHOT_TIMER = 0x04,
			SET_SEND_TIMER = 0x05,
			QUEUE_REQUEST = 0x06,
			STOP = 0xFF
		};
		static semaphor_manager& getInstance();
		void create_semaphor();
		void add_semaphor(uint16_t);
		void print_sem_list();
		uint8_t read_s_state(uint16_t);

		//Сохранить своё состояние(светофор->менеджер)
		void load_s_state(uint16_t, uint8_t);

		//Сохранить счетчик очереди
		void load_s_queue(uint16_t, uint16_t);

		//Управление потоком
		void disallowTransit(uint16_t);
		void allowTransit(uint16_t);

		//Установка соседей
		void set_neighbour(uint16_t target_sem, boost::container::vector<uint16_t> &n_id);

		//Запрос состояния соседа
		uint8_t get_neighbour_state(uint16_t);

		//Добавить очередь перед светофором
		void addSemaphorQueue(uint16_t);
		void addSemaphorQueue(uint16_t, uint8_t);

		//Увеличить очередь перед светофором на 1
		void increment_semaphor_queue(uint16_t);

		//Уменьшить очередь перед светофором на 1
		void decrement_semaphor_queue(uint16_t);

		//Генератор очередей перед светофором
		void run_queue_generator(bool mode);

		//Менеджер светофоров
		void run_manager();

		//Запрос состояния конкретного светофора
		uint8_t semaphor_request(uint16_t sem_id, uint8_t code);
		
		//Общий запрос всем светофорам
		void semaphor_request(uint8_t code);

		//Отправка команды конкретному светоформу
		bool send_command_to_target_semaphor(uint16_t sem_id, uint8_t code);

		//Просчет очереди на пропуск авто
		void calc_transit_priority();

		//Установка имени светофора
		void set_semaphor_name(uint16_t sem_id, std::string name);

		//Рассчитываем порядок проезда
		bool query_transit(uint16_t sender_id);

		//Добавляем светофоры в карту
		void add_semaphor_to_map(boost::container::vector<uint16_t> map);

		//Добавляем список одновременно разрешенных зон
		void add_parallel_zones(boost::container::vector<uint8_t> zone_lst);

		//Задаем график зависимости очередь<->частота опроса
		void set_polling_graph(std::pair<uint8_t, uint8_t>);

		//Подсчитываем количество циклов обновления исходя из загруженности
		void calc_udpate_cycle();

		//Копируем график в светофор
		void copy_polling_graph(uint16_t sem_id);

		//Останавливаем потоки
		void stop_all_threads();

		//Записываем частоту генератора
		void set_gen_freq(int freq);

		//Останавливаем генератор
		void stop_generator();
	signals:
		void send_msg(QString);
};