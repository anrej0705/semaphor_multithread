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
#include <qdom.h>
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
		QDomDocument* xml_set_document;
		QDomElement* semaphor_speed_cycles;	//Длина цикла в мс
		QDomElement* generator_speed_cycles;	//Длина мс
		QDomElement* manager_speed_cycles;	//Длина в мс
		QDomElement* gui_refresh_speed_cycles;	//Длина в мс
		QDomElement* gui_refresh_interval;	//Длина в циклах
		QDomElement* car_pass;	//Длина в циклах
		QDomElement* tcp_port_n;	//Длина в циклах
		QDomElement* theme_n;
		QFile* xml_set_file;

		uint16_t vesrion_number;
		uint16_t generator_timer_speed;
		uint16_t semaphor_timer_speed;
		uint16_t manager_speed_cycle;
		uint16_t gui_refresh_speed_cycle;
		uint16_t gui_refresh_interval_cycle;
		uint16_t car_pass_in_one_iteration;
		uint16_t tcp_port;
		uint16_t theme_code;

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
		uint16_t semaphor_speed;
		enum// class status_codes	//fix C26812
		{
			STATUS_REQUEST = 0x01,
			TRANSIT_ALLOWED = 0x02,
			TRANSIT_DISALLOWED = 0x03,
			SET_SHOT_TIMER = 0x04,
			SET_SEND_TIMER = 0x05,
			QUEUE_REQUEST = 0x06,
			SET_CYCLE_SPEED = 0x07,
			SET_CYCLE_MS = 0x08,
			STOP = 0xFF
		};
		static semaphor_manager& getInstance();
		void create_semaphor();
		void add_semaphor(const uint16_t&);
		void print_sem_list();
		uint8_t read_s_state(const uint16_t&);

		//Сохранить своё состояние(светофор->менеджер)
		void load_s_state(const uint16_t&, const uint8_t&);

		//Сохранить счетчик очереди
		void load_s_queue(const uint16_t&, uint16_t);

		//Управление потоком
		void disallowTransit(const uint16_t&);
		void allowTransit(const uint16_t&);

		//Установка соседей
		void set_neighbour(const uint16_t& target_sem, const boost::container::vector<uint16_t> &n_id);

		//Запрос состояния соседа
		uint8_t get_neighbour_state(const uint16_t&);

		//Добавить очередь перед светофором
		void addSemaphorQueue(const uint16_t&);
		void addSemaphorQueue(const uint16_t&, const uint8_t&);

		//Увеличить очередь перед светофором на 1
		void increment_semaphor_queue(const uint16_t&);

		//Уменьшить очередь перед светофором на 1
		void decrement_semaphor_queue(const uint16_t&);

		//Генератор очередей перед светофором
		void run_queue_generator(const bool& mode);

		//Менеджер светофоров
		void run_manager();

		//Запрос состояния конкретного светофора
		uint8_t semaphor_request(const uint16_t& sem_id, const uint8_t& code);
		
		//Общий запрос всем светофорам
		void semaphor_request(uint8_t code);

		//Отправка команды конкретному светоформу
		bool send_command_to_target_semaphor(const uint16_t& sem_id, const uint8_t& code);

		//Просчет очереди на пропуск авто
		void calc_transit_priority();

		//Установка имени светофора
		void set_semaphor_name(const uint16_t& sem_id, const std::string& name);

		//Рассчитываем порядок проезда
		bool query_transit(const uint16_t& sender_id);

		//Добавляем светофоры в карту
		void add_semaphor_to_map(const boost::container::vector<uint16_t>& map);

		//Добавляем список одновременно разрешенных зон
		void add_parallel_zones(const boost::container::vector<uint8_t>& zone_lst);

		//Задаем график зависимости очередь<->частота опроса
		void set_polling_graph(const std::pair<uint8_t, uint8_t>&);

		//Подсчитываем количество циклов обновления исходя из загруженности
		void calc_udpate_cycle();

		//Копируем график в светофор
		void copy_polling_graph(const uint16_t& sem_id);

		//Останавливаем потоки
		void stop_all_threads();

		//Записываем частоту генератора
		void set_gen_freq(const int& freq);

		//Останавливаем генератор
		void stop_generator();

		//Задаем скорость работы светофров
		void set_semaphor_speed(const uint16_t& speed);

		//Генератор XML документа
		QDomElement parametr(QDomDocument& d_doc, const QString& param_name, const QString& param_value);

		//Генератор XML строк
		//QDomElement make_element(QDomDocument& d_doc, const QString& str_name, const QString& str_attr, const QString& str_text);

		//Получаем значения скорости цикла
		uint16_t get_cycle_timer_speed_ms();

		//Читаем значение периода обновления графики
		uint8_t read_update_interval_period();

		//Грузим настройки из XML
		void read_xml();

		//Запускаем потоки(сбрасываем флаг semaphor_wait в каждом)
		void run_all_semaphors();
	signals:
		void send_msg(QString);
};