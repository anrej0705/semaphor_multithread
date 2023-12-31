#pragma once
#include <stdint.h>
#include <vector>
#include <thread>
#include <string>
#include "boost/container/vector.hpp"
#include <QtWidgets>
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
		uint16_t myId;	//�� ���������
		uint8_t status;	//������� ���������
		uint8_t cycle_timer;
		boost::container::vector<uint8_t> neighbout_reg_state;	//��������� ��������� �������
		boost::container::vector<uint16_t>* neighbour;	//��������� ��� �������� �� �������(���� ������ ����)
		boost::container::vector<std::pair<uint8_t, uint8_t>>* polling_graph;
		uint16_t waitingTimer;	//������ �������� � ������, ������� ����������
		uint16_t roadscanTimer;	//������ ������ ������ ����� �����
		uint16_t transitionAllowTimer;	//������ ���������� �������
		std::thread *sThread;
		bool roadBusy;	//���� ������� ������
	public:
		bool stop_thread;

		volatile uint16_t manager_query_code;	//��� ������� �� ���������
		volatile uint16_t queue;	//������� �� �����������
		volatile bool transitAllowed;	//����, ����������� ������������ ��������� � �����
		semaphor();
		~semaphor();
		void run_thread();
		void new_thread(uint16_t);
		void test_hello();

		//������ ������
		void add_neighbour(uint16_t n_id);

		//��� ���������. ������ ��������� ���������
		uint8_t get_reg_state();

		//������ ��������� �������
		void request_neighbour_status();

		//��������� ��� � �������(���� ���� ���� ������� �� �� ����)
		bool road_safety_check();

		//������� ����������
		void car_pass();

		//��������� ����� ���������
		void set_name(std::string name);

		//������ ������� �� ���������
		void read_manager_command();

		//��������� ������ ������� ����������� �������� 
		void set_polling_graph(std::pair<uint8_t, uint8_t>);

		//������������ ������� ������ ���������
		void calc_query_cycle();

		//�������� �������� ������� ������
		uint8_t get_cycle_timer_value();

	signals:
		//������ ��� �������� ���������� � ���������
		void signal_send_info(std::pair<uint16_t, uint16_t>);
};
