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
		//std::vector<std::pair<uint8_t, uint8_t>> *transit_order;	//������� ��������� ��������� UPD --> std::map
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

		//��������� ��� ���������(��������->��������)
		void load_s_state(uint16_t, uint8_t);

		//��������� ������� �������
		void load_s_queue(uint16_t, uint16_t);

		//���������� �������
		void disallowTransit(uint16_t);
		void allowTransit(uint16_t);

		//��������� �������
		void set_neighbour(uint16_t target_sem, boost::container::vector<uint16_t> &n_id);

		//������ ��������� ������
		uint8_t get_neighbour_state(uint16_t);

		//�������� ������� ����� ����������
		void addSemaphorQueue(uint16_t);
		void addSemaphorQueue(uint16_t, uint8_t);

		//��������� ������� ����� ���������� �� 1
		void increment_semaphor_queue(uint16_t);

		//��������� ������� ����� ���������� �� 1
		void decrement_semaphor_queue(uint16_t);

		//��������� �������� ����� ����������
		void run_queue_generator(bool mode);

		//�������� ����������
		void run_manager();

		//������ ��������� ����������� ���������
		uint8_t semaphor_request(uint16_t sem_id, uint8_t code);
		
		//����� ������ ���� ����������
		void semaphor_request(uint8_t code);

		//�������� ������� ����������� ����������
		bool send_command_to_target_semaphor(uint16_t sem_id, uint8_t code);

		//������� ������� �� ������� ����
		void calc_transit_priority();

		//��������� ����� ���������
		void set_semaphor_name(uint16_t sem_id, std::string name);

		//������������ ������� �������
		bool query_transit(uint16_t sender_id);

		//��������� ��������� � �����
		void add_semaphor_to_map(boost::container::vector<uint16_t> map);

		//��������� ������ ������������ ����������� ���
		void add_parallel_zones(boost::container::vector<uint8_t> zone_lst);

		//������ ������ ����������� �������<->������� ������
		void set_polling_graph(std::pair<uint8_t, uint8_t>);

		//������������ ���������� ������ ���������� ������ �� �������������
		void calc_udpate_cycle();

		//�������� ������ � ��������
		void copy_polling_graph(uint16_t sem_id);

		//������������� ������
		void stop_all_threads();

		//���������� ������� ����������
		void set_gen_freq(int freq);

		//������������� ���������
		void stop_generator();
	signals:
		void send_msg(QString);
};