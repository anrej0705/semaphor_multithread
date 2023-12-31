#include "manager.h"
#include "semaphor.h"
#include "gui.h"
#include "boost/thread/thread.hpp"
#include "boost/chrono/chrono.hpp"
#include "boost/container/vector.hpp"
#include "boost/fusion/include/pair.hpp"
#include <ranges>
#include <algorithm>
#include <iostream>
#include <QtWidgets>
#include <mutex>
#include <condition_variable>
#include <map>

//��������� �������
#include <random>
#include <ctime>
std::mt19937 randon_engine;
std::condition_variable condition;
std::mutex m_mut;

//��� ���� ������ ������ � ������
#define THREAD_LOCK std::lock_guard<std::mutex> lg(m_mut);
#define NOTIFY_ALL_THREAD condition.notify_all();

semaphor_manager* semaphor_manager::manager_ptr = 0;
singlet_destroyer semaphor_manager::dstr;

/*/
/�����, ������������� �������� ����� ������ �� �����
/*/
singlet_destroyer::~singlet_destroyer()
{
	if(m_inst!=NULL)
		delete m_inst;
}
void singlet_destroyer::initialize(semaphor_manager* ptr)
{
	m_inst = ptr;
}

/*/
/������� �����������
/�������������� �������� �� ���������� � ���������� ������� ������������� ����������
/*/
semaphor_manager::semaphor_manager()
{
	generator_thread = nullptr;	//fix C26495
	manager_thread = nullptr;	//fix C26495
	s_id = new boost::container::vector<uint16_t>;
	reg_s_state = new boost::container::vector<uint8_t>;
	zone_list = new boost::container::vector<boost::container::vector<uint8_t>>;
	//queue_list = new std::vector<std::pair<uint16_t, uint16_t>>;
	queue_list = new std::map<uint16_t, uint16_t>;
	semaphor_map = new std::map<uint16_t, boost::container::vector<uint16_t>>;
	//transit_order = new std::vector<std::pair<uint8_t, uint8_t>>; // --> std::map, ������ ��������
	polling_graph = new boost::container::vector<std::pair<uint8_t, uint8_t>>;
	generator_mode = 0;
	semaphors_cnt = 0;
	calc = 0;
	semaphors_map_size = 0;
	zone = 0;
	n_zone = 0;
	manager_cycle = 0;
	stop_thread = 0;
	queue_delay = 1;
}
//������ ����������
semaphor_manager::~semaphor_manager()
{}
semaphor_manager& semaphor_manager::getInstance()
{
	if (!manager_ptr)
	{
		manager_ptr = new semaphor_manager();
		dstr.initialize(manager_ptr);
	}
	return *manager_ptr;
}
/*/
/����� ��������� ��������� �������� � ������ �������������
/����� ������� ����� ��������� ������������ �������� ����������
/�� ���� �������� ������ ���������� �� ������ ����������� ��
/������ � ������ �� ����� �����������, ������� �� ������� ��
/*/
void semaphor_manager::add_semaphor(uint16_t sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	semaphor_gui::getInstance().slot_post_console_msg("[LOG] New semaphor with id = " + sem_id);
	qDebug() << "[LOG] New semaphor with id =" << sem_id;
	s_id->push_back(sem_id);
	reg_s_state->push_back(0x00);	//��������� ��������� ����������, ������� 0, �� ���� �������� ��� �� ���� �� �������
	++semaphors_cnt;
	//reg_s_state->resize(semaphors_cnt); //��������� �����
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}

/*/
/��������� ��������� ��������� � ����. �������� ����� ����� ����� ����� �������� ��� ���������
/�� �������� ����� ������� ������ ��������� �� ������������� ������
/*/
void semaphor_manager::load_s_state(uint16_t sem_id, uint8_t s_state)
{
	THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock(); //������������ �����������
	qDebug() << "[LOG] Semaphor id =" << sem_id << ", state =" << s_state;
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		return;	//���� ������ ������ ��� �� ����� �������
	}
	uint16_t index = std::distance(s_id->begin(), it);

	reg_s_state->at(index) = s_state;	//��������� ���������, ������� ������� ��������

	/*auto match = *s_id | std::views::filter([&sem_id](auto& v) {
		return v == sem_id;
		});*/	//��� ������ ���-�� ����, �� ��������� - C++20 ����, ����� � ��������� �����
	/*uint16_t s_index = 0;
	for (uint16_t a = 0; a < static_cast<uint16_t>(s_id->size()); ++a)
	{
		if (s_id[a]==sem_id)
		{
			//�� �������� � ������������������� �� ��������� ���������
		}
	}*/

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

/*/
/����� �� ������� ��������� ���� ����������, ��� ������
/*/
void semaphor_manager::print_sem_list()
{
	qDebug() << "[LOG] print all semaphore state";
	for (uint8_t a = 0; a < static_cast<uint8_t>(s_id->size()); ++a)
	{
		qDebug() << "[LOG] Semaphore id = " << s_id->at(a) << " state = " << reg_s_state->at(a);
	}
}

/*/
/������� ���������� ����������� � ����������� ���� ��������� ���������
/*/
uint8_t semaphor_manager::read_s_state(uint16_t sem_id)
{
	THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//���� ����������� �� �����
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		//ul.unlock();
		return 0;	//���� ������ ������ ��� �� ����� �������
	}
	uint16_t index = std::distance(s_id->begin(), it);

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
	return reg_s_state->at(index);	//��������� ���������, ������� ������� ��������
}

void semaphor_manager::create_semaphor()
{
	semaphor_gui::getInstance().slot_post_console_msg("[LOG]Adding semaphore");
	qDebug() << "[LOG]Adding semaphore";
	++semaphors_cnt;
	s_list.resize(semaphors_cnt);
	s_queue.push_back(0);
	s_id->push_back(semaphors_cnt - 1);
	reg_s_state->push_back(0x00);
	s_list[semaphors_cnt - 1] = new semaphor;
	s_list[semaphors_cnt - 1]->new_thread(semaphors_cnt-1);
}

//���������� ������� �����
//������ ��� ����������
void semaphor_manager::allowTransit(uint16_t sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	s_list[sem_id]->manager_query_code = 0x02;
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}
void semaphor_manager::disallowTransit(uint16_t sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	s_list[sem_id]->manager_query_code = 0x03;
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}

void semaphor_manager::addSemaphorQueue(uint16_t sem_id)
{
	//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Increment queue to id " + QString::number(sem_id));
	//qDebug() << "[LOG] Increment semaphor id =" << sem_id << "queue +1";
	++s_list[sem_id]->queue;
}

void semaphor_manager::addSemaphorQueue(uint16_t sem_id, uint8_t q_size)
{
	//qDebug() << "[LOG] Increment semaphor id =" << sem_id << "queue +=" << q_size;
	s_list[sem_id]->queue += q_size;
}

void semaphor_manager::decrement_semaphor_queue(uint16_t sem_id)
{
	if (s_list[sem_id]->queue == 0)
		return;
	semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Decrement queue to id " + QString::number(sem_id) + "(" + QString::number(s_list[sem_id]->queue) + " -> " + QString::number(s_list[sem_id]->queue - 1) + ")");
	qDebug() << "[LOG]Decrement semaphor id =" << sem_id << "queue +1";
	s_list[sem_id]->queue--;
}

void semaphor_manager::increment_semaphor_queue(uint16_t sem_id)
{
	semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Increment queue to id " + QString::number(sem_id) + "(" + QString::number(s_list[sem_id]->queue) + " -> " + QString::number(s_list[sem_id]->queue+1) + ")");
	qDebug() << "[LOG]Increment semaphor id =" << sem_id << "queue +1";
	s_list[sem_id]->queue++;
}

void semaphor_manager::set_neighbour(uint16_t target_sem, boost::container::vector<uint16_t>& n_id)
{
	for (uint8_t a = 0; a < static_cast<uint8_t>(n_id.size()); ++a)
	{
		//qDebug() << "[MANAGER]Target id =" << target_sem << "Neighbour id = " << n_id.at(a);
		s_list[target_sem]->add_neighbour(n_id.at(a));
	}
}

//����������� ��������� � ��������� � ���������� �����
uint8_t semaphor_manager::get_neighbour_state(uint16_t n_id)
{
	return s_list[n_id]->get_reg_state();
}

void semaphor_manager::run_queue_generator(bool mode)
{
	if (mode)
	{
		generator_mode = 1;
		generator_thread = new boost::thread(&semaphor_manager::queueGenerator, this);
		generator_thread->detach();
	}
}

void semaphor_manager::queueGenerator()
{
	QString logout;
	uint16_t semaphor_id = 0;
	uint8_t cycle_cnt = 0;
	while (1)
	{
		if (cycle_cnt == queue_delay)
		{
			semaphor_id = rand() % static_cast<uint16_t>(s_list.size()) + 0;
			addSemaphorQueue(semaphor_id);
			cycle_cnt = 0;
		}
		if (stop_thread)
			break;
		//qDebug() << "[EVENT QUEUE]Cycle" << cycle_cnt;
		//logout += "[GENERATOR THREAD] Added +1 queue to random semaphor(id=" + QString::number(semaphor_id) + ")";
		if (!generator_mode)
		{
			generator_mode = 1;
			break;
		}
		//qDebug() << logout;
		//logout.clear();
		++cycle_cnt;

		boost::this_thread::sleep_for(boost::chrono::milliseconds(4));
	}
}

void semaphor_manager::run_manager()
{
	manager_thread = new boost::thread(&semaphor_manager::queueManager, this);
	manager_thread->detach();
}

void semaphor_manager::queueManager()
{
	uint8_t cycle_cnt = 0;
	while (1)
	{
		if (stop_thread)
		{
			//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Stopping manager thread");
			break;
		}
		//qDebug() << "[QUEUE]Cycle" << cycle_cnt;
		//������ ������� ��������� ���� �������������� ����������

		if (cycle_cnt == manager_cycle)
		{
			calc_transit_priority();
			calc_udpate_cycle();
			cycle_cnt = 0;
		}

		++cycle_cnt;

		QApplication::processEvents();

		boost::this_thread::sleep_for(boost::chrono::microseconds(10));
	}
}

void semaphor_manager::calc_transit_priority()
{
	boost::container::vector<uint16_t> _s_id = *s_id;
	boost::container::vector<uint16_t> _s_queue = s_queue;
	boost::container::vector<uint16_t>::iterator it;
	int zone_num = 0;
	QString logout;
	uint16_t max_size_element = 0;
	uint16_t s_count = static_cast<uint16_t>(_s_id.size());
	//qDebug() << "[MANAGER]Calc transit order";
	queue_list->clear();
	while (1)
	{
		if (s_count == 0)
			break;
		//qDebug() << "[MANAGER]Calc cycle" << QString::number(s_count);
		it = std::max_element(_s_queue.begin(), _s_queue.end());	//�������� ��������� �� ������� � ������������ ��������
		//qDebug() << "[MANAGER]Max element find =" << QString::number(_s_queue.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))));

		//���������� � ����� � ��������� � ID ���������
		/*queue_list->push_back(
			std::pair<uint16_t, uint16_t>(static_cast<uint16_t>(s_id->size()) - s_count,
			_s_id.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))))
		);*/
		queue_list->insert(
			std::pair<uint16_t, uint16_t>(static_cast<uint16_t>(s_id->size())-s_count, 
			_s_id.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))))
		);
		_s_queue[s_id->at(static_cast<int>(std::distance(_s_queue.begin(), it)))] = 0;	//�������� �������� ����� �� ���������� ��������
		--s_count;
	}
	qDebug() << "[MANAGER]Print ordered list";
	qDebug() << " _____________________________________________________________________________";
	for (auto it_map = queue_list->cbegin(); it_map != queue_list->cend(); ++it_map)
	{
		if (it_map->first == 0)	//����� �������������� ��������� � ������ ����������� � �����. ����
			for (auto _it = semaphor_map->cbegin(); _it != semaphor_map->cend(); ++_it)
				for (uint8_t m_cnt = 0; m_cnt < _it->second.size(); ++m_cnt)
					if (it_map->second == _it->second[m_cnt])
						for (uint8_t z_lst_it = 0; z_lst_it < zone_list->size(); ++z_lst_it)
							for (uint8_t z_l_pos_it = 0; z_l_pos_it < zone_list->at(z_lst_it).size(); ++z_l_pos_it)
								if (_it->first == zone_list->at(z_lst_it).at(z_l_pos_it))
									n_zone = z_lst_it;
		semaphor_gui::getInstance().write_table_content(it_map->first, 0, it_map->first);
		semaphor_gui::getInstance().write_table_content(it_map->first, 1, it_map->second);
		semaphor_gui::getInstance().write_table_content(it_map->first, 2, s_queue.at(it_map->second));
		logout += "| Priority = " + QString::number(it_map->first).leftJustified(2, ' ')
			+ " | ID = " + QString::number(it_map->second).leftJustified(2, ' ')
			+ " | queue size = " + QString::number(s_queue.at(it_map->second)).leftJustified(2, ' ')
			+ " | zone ";	//����� ���� ���� ��������� ������ �� ��� ������� ������� � ����� ������ ����� �� ������
		//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Max queue size = " + QString::number(s_queue.at(it_map->second)).leftJustified(2, ' '));
		for (uint8_t z_lst_it = 0; z_lst_it < zone_list->size(); ++z_lst_it)
		{
			for (uint8_t z_sublst_it = 0; z_sublst_it < zone_list->at(z_lst_it).size(); ++z_sublst_it)
			{
				auto zone_map_it = semaphor_map->cbegin();
				std::advance(zone_map_it, zone_list->at(z_lst_it).at(z_sublst_it));
				for (uint8_t b = 0; b < zone_map_it->second.size(); ++b)
				{
					//qDebug() << "ID" << QString::number(zone_map_it->second.at(b)) << "zone list" << QString::number(z_lst_it) << "zone sublist" << QString::number(zone_list->at(z_lst_it).at(z_sublst_it)) << "Target ID" << QString::number(it_map->second);
					if (it_map->second == zone_map_it->second.at(b))
					{
						//qDebug() << "Accp" << QString::number(zone_list->at(z_lst_it).at(z_sublst_it));
						logout += QString::number(zone_list->at(z_lst_it).at(z_sublst_it));
						semaphor_gui::getInstance().write_table_content(it_map->first, 3, zone_list->at(z_lst_it).at(z_sublst_it));
					}
				}
			}
		}
		logout += " | cycle timer " + QString::number(s_list[it_map->second]->get_cycle_timer_value()).leftJustified(2, ' ') + " times |";
		semaphor_gui::getInstance().write_table_content(it_map->first, 4, s_list[it_map->second]->get_cycle_timer_value());
		qDebug() << logout;
		logout.clear();
	}
	//semaphor_gui::getInstance().slot_post_console_msg("[LOG]Stored zone list num" + QString::number(n_zone).leftJustified(2, ' '));
	qDebug() << "[LOG]Stored zone list num" << QString::number(n_zone).leftJustified(2, ' ');
	//queue_list->clear();	//�� ����
}

uint8_t semaphor_manager::semaphor_request(uint16_t sem_id, uint8_t code)
{
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//���� ����������� �� �����
	uint8_t response_code = 0x00;
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return 0;	//���� ������ ������ ��� �� ����� �������
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);

	response_code = s_list[index]->get_reg_state();

	//qDebug() << "[LOG][semaphor_request]Semaphor id =" << sem_id << "returned response status=" << response_code;

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();

	return response_code;
}

void semaphor_manager::semaphor_request(uint8_t code)
{
	THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	for (uint8_t a = 0; a < static_cast<uint8_t>(s_list.size()); ++a)
	{
		s_list[a]->manager_query_code = code;
	}
	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

//��������� ������� ������� ���������, ���������� ���� �����
void semaphor_manager::load_s_queue(uint16_t sem_id, uint16_t queue_size)
{
	//qDebug() << "[MANAGER]Semaphor id =" << QString::number(sem_id) << " queue size =" << QString::number(queue_size);
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//���� ����������� �� �����
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return;	//���� ������ ������ ��� �� ����� �������
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);

	s_queue[index] = queue_size;
	//qDebug() << "[LOG][load_s_queue]Save queue size =" << s_queue[index] << "from semaphor id =" << sem_id;

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

bool semaphor_manager::send_command_to_target_semaphor(uint16_t sem_id, uint8_t code)
{
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return 0;	//���� ������ ������ ��� �� ����� �������
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);
	s_list[index]->manager_query_code = code;

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
	return 1;
}

void semaphor_manager::set_semaphor_name(uint16_t sem_id, std::string name)
{
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return;	//���� ������ ������ ��� �� ����� �������
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);
	s_list[index]->set_name(name);

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

bool semaphor_manager::query_transit(uint16_t sender_id)
{
	if (queue_list->size() == 0)
		return 0;

	THREAD_LOCK

	for (uint8_t a = 0; a < zone_list->at(n_zone).size(); ++a)
	{
		//qDebug() << "Zone" << QString::number(zone_list->at(n_zone).at(a));
		//qDebug() << "  Semaphors id`s in zone" << QString::number(zone_list->at(n_zone).at(a));
		auto zone_map_it = semaphor_map->cbegin();
		std::advance(zone_map_it, zone_list->at(n_zone).at(a));
		for (uint8_t b = 0; b < zone_map_it->second.size(); ++b)
		{
			//qDebug() << "    ID" << QString::number(zone_map_it->second.at(b));
			if (sender_id == zone_map_it->second.at(b))
			{
				//qDebug() << "      ID" << QString::number(zone_map_it->second.at(b)) << "accepted";
				NOTIFY_ALL_THREAD
				return 1;
			}
		}
	}
	NOTIFY_ALL_THREAD
	return 0;
}

void semaphor_manager::add_semaphor_to_map(boost::container::vector<uint16_t> map)
{
	semaphor_map->insert(std::pair(semaphors_map_size, map));
	semaphor_gui::getInstance().slot_post_console_msg("[LOG]Import semaphor list");
	qDebug() << "[LOG]Import semaphor list";
	for (uint8_t a = 0; a < static_cast<uint8_t>(map.size()); ++a)
	{
		semaphor_gui::getInstance().slot_post_console_msg("[VECTOR]Semaphor id = " + QString::number(map[a]).leftJustified(2, ' ') + " zone = " + QString::number(semaphors_map_size).leftJustified(2, ' '));
		qDebug() << "[VECTOR]Semaphor id =" << QString::number(map[a]).leftJustified(2, ' ') << "zone =" << QString::number(semaphors_map_size).leftJustified(2, ' ');
	}
	++semaphors_map_size;
}

void semaphor_manager::add_parallel_zones(boost::container::vector<uint8_t> zone_lst)
{
	semaphor_gui::getInstance().slot_post_console_msg("[LOG]Add zone list");
	qDebug() << "[LOG]Add zone list";
	for (uint8_t a = 0; a < zone_lst.size(); ++a)
	{
		semaphor_gui::getInstance().slot_post_console_msg("[IMPORT] import zone" + QString::number(zone_lst[a]));
		qDebug() << "[IMPORT] import zone" << QString::number(zone_lst[a]);
	}
	zone_list->push_back(zone_lst);
}

void semaphor_manager::set_polling_graph(std::pair<uint8_t, uint8_t> section)
{
	qDebug() << "[LOG]Add section {" << QString::number(section.first) << QString::number(section.second) << "}";
	polling_graph->push_back(std::pair<uint8_t,uint8_t>(section.first,section.second));
}

void semaphor_manager::calc_udpate_cycle()
{
	uint16_t max_queue = 0;
	qDebug() << "[LOG]Calc update cycle, current cycle" << QString::number(manager_cycle) << "times";
	for (auto it_map = queue_list->cbegin(); it_map != queue_list->cend(); ++it_map)
	{
		if (max_queue < s_queue.at(it_map->second))
			max_queue = s_queue.at(it_map->second);
	}
	qDebug() << "[LOG]Max queue =" << QString::number(max_queue);
	for (uint8_t a = 0; a < polling_graph->size(); ++a)
	{
		//qDebug() << "[LOG]Cycle length" << QString::number(cycle_length) << QString::number(polling_graph->at(a).first) << QString::number(max_queue);
		if (polling_graph->at(a).first >= max_queue)
		{
			manager_cycle = polling_graph->at(a).second;
			qDebug() << "[LOG]Set cycle length" << QString::number(manager_cycle);
			break;
		}
	}
	//qDebug() << "[LOG]Cycle length" << QString::number(cycle_length);
}

void semaphor_manager::copy_polling_graph(uint16_t sem_id)
{
	THREAD_LOCK

	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		//ul.unlock();
		return;	//���� ������ ������ ��� �� ����� �������
	}
	uint16_t index = std::distance(s_id->begin(), it);

	for (uint8_t a = 0; a < polling_graph->size(); ++a)
	{
		s_list[sem_id]->set_polling_graph(polling_graph->at(a));
	}

	NOTIFY_ALL_THREAD
}

void semaphor_manager::stop_all_threads()
{
	for (uint8_t a = 0; a < s_list.size(); ++a)
	{
		s_list.at(a)->stop_thread = 1;
		//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Stopping semaphor id = " + QString::number(a));
	}
	stop_thread = 1;
}

void semaphor_manager::set_gen_freq(int freq)
{
	queue_delay = 167 - freq;
}

void semaphor_manager::stop_generator()
{
	generator_mode = 0;
}
