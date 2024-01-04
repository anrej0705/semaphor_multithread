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

//Генерация рандома
#include <random>
#include <ctime>
std::mt19937 randon_engine;
std::condition_variable condition;
std::mutex m_mut;

//Эту дичь пихать только в методы
#define THREAD_LOCK std::lock_guard<std::mutex> lg(m_mut);
#define NOTIFY_ALL_THREAD condition.notify_all();

semaphor_manager* semaphor_manager::manager_ptr = 0;
singlet_destroyer semaphor_manager::dstr;

/*/
/Класс, гарантирующий удаление после выхода из проги
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
/Базовый конструктор
/Инициализирует элементы по указателям и сбрасывает счетчик обслуживаемых светофоров
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
	//transit_order = new std::vector<std::pair<uint8_t, uint8_t>>; // --> std::map, только красивее
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
	semaphor_speed = 1;
	
	//Параметры по умолчанию
	semaphor_timer_speed = 42;
	generator_timer_speed = 4;
	manager_speed_cycle = 10;
	gui_refresh_speed_cycle = 50;
	gui_refresh_interval_cycle = -1;
}
//Пустой деструктор
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
/Метод добавляет указанный светофор в список обслуживаемых
/Таким образом можно исключить обслуживание ненужных светофоров
/То есть например массив светофором на другом перекрестке не
/Входит в список на нашем перекрестке, поэтому не трогаем их
/*/
void semaphor_manager::add_semaphor(const uint16_t& sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[LOG] New semaphor with id = ") + sem_id);
	//qDebug() << "[LOG] New semaphor with id =" << sem_id;
	s_id->push_back(std::move(sem_id));
	reg_s_state->push_back(0x00);	//Состояние светофора неизвестно, оставим 0, то есть светофор ещё ни разу не читался
	++semaphors_cnt;
	//reg_s_state->resize(semaphors_cnt); //оставляет мусор
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}

/*/
/Сохраняет состояние светофора у себя. Светофор может после этого сразу поменять своё состояние
/Но менеджер будет хранить старое состояние до востребования нового
/*/
void semaphor_manager::load_s_state(const uint16_t& sem_id, const uint8_t& s_state)
{
	THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock(); //Велосипедная атомарность
	//qDebug() << "[LOG] Semaphor id =" << sem_id << ", state =" << s_state;
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		return;	//Если ничего такого нет то сразу выходим
	}
	uint16_t index = std::distance(s_id->begin(), it);

	reg_s_state->at(index) = std::move(s_state);	//Сохраняем состояние, которое передал светофор

	/*auto match = *s_id | std::views::filter([&sem_id](auto& v) {
		return v == sem_id;
		});*/	//Тут дальше как-то надо, не сообразил - C++20 фича, освою в ближайшее время
	/*uint16_t s_index = 0;
	for (uint16_t a = 0; a < static_cast<uint16_t>(s_id->size()); ++a)
	{
		if (s_id[a]==sem_id)
		{
			//Не работает с инициализированными по указателю векторами
		}
	}*/

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

/*/
/Вывод на консоль состояния всех светофоров, для дебага
/*/
void semaphor_manager::print_sem_list()
{
	//qDebug() << "[LOG] print all semaphore state";
	for (uint8_t a = 0; a < static_cast<uint8_t>(s_id->size()); ++a)
	{
		//qDebug() << "[LOG] Semaphore id = " << s_id->at(a) << " state = " << reg_s_state->at(a);
	}
}

/*/
/Возврат последнего записанного в собственную базу состояния светофора
/*/
uint8_t semaphor_manager::read_s_state(const uint16_t& sem_id)
{
	THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//Типа атомарность всё такое
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		//ul.unlock();
		return 0;	//Если ничего такого нет то сразу выходим
	}
	uint16_t index = std::distance(s_id->begin(), it);

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
	return reg_s_state->at(index);	//Сохраняем состояние, которое передал светофор
}

/*
* Создание объекта светофора и перемещение в отдельный поток
*/
void semaphor_manager::create_semaphor()
{
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[LOG]Adding semaphore"));
	//qDebug() << "[LOG]Adding semaphore";
	++semaphors_cnt;
	s_list.resize(semaphors_cnt);	//Инициализируемся
	s_queue.push_back(0);
	s_id->push_back(semaphors_cnt - 1);
	reg_s_state->push_back(0x00);

	//Создаем и перемещаем в отдельный поток
	s_list[semaphors_cnt - 1] = new semaphor;
	s_list[semaphors_cnt - 1]->new_thread(semaphors_cnt-1);
}

//Управление потоком машин
//Запрет или разрешение
void semaphor_manager::allowTransit(const uint16_t& sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	s_list[sem_id]->manager_query_code = 0x02;	//Разрешаем движение(вручную)
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}
void semaphor_manager::disallowTransit(const uint16_t& sem_id)
{
	//std::lock_guard<std::mutex> lg(m_mut);
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();
	s_list[sem_id]->manager_query_code = 0x03;	//Запрещаем движение(вручную)
	//m_mut.unlock();
	//condition.notify_all();
	//ul.unlock();
}

//Добавить +1 к очереди машин перед светофором выбранному по ИД
void semaphor_manager::addSemaphorQueue(const uint16_t& sem_id)
{
	//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Increment queue to id " + QString::number(sem_id));
	//qDebug() << "[LOG] Increment semaphor id =" << sem_id << "queue +1";
	++s_list[sem_id]->queue;
}

//Добавить очередь из 2 и более машин перед светофром с выбранным ИД
void semaphor_manager::addSemaphorQueue(const uint16_t& sem_id, const uint8_t& q_size)
{
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[MANAGER]Set queue size ") + QString::number(q_size));
	//qDebug() << "[LOG] Increment semaphor id =" << sem_id << "queue +=" << q_size;
	s_list[sem_id]->queue += std::move(q_size);	//C26478 ложное срабатывание
}

/*
* Добавить/убавить очередь машина на 1 перед светофром с выбранным ИД
*/
void semaphor_manager::decrement_semaphor_queue(const uint16_t& sem_id)
{
	if (s_list[sem_id]->queue == 0)	//Если машин нету то выходим сразу
		return;
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[MANAGER]Decrement queue to id ") + QString::number(sem_id) + "(" + QString::number(s_list[sem_id]->queue) + " -> " + QString::number(s_list[sem_id]->queue - 1) + ")");
	//qDebug() << "[LOG]Decrement semaphor id =" << sem_id << "queue +1";
	s_list[sem_id]->queue--;
}

void semaphor_manager::increment_semaphor_queue(const uint16_t& sem_id)
{
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[MANAGER]Increment queue to id ") + QString::number(sem_id) + "(" + QString::number(s_list[sem_id]->queue) + " -> " + QString::number(s_list[sem_id]->queue+1) + ")");
	//qDebug() << "[LOG]Increment semaphor id =" << sem_id << "queue +1";
	s_list[sem_id]->queue++;
}

/*
* Установить соседей для светофора
* 
* Светофор будет ориентироваться на показания соседей чтобы принять решение - пропустить или не пропускать машину/пешехода
*/
void semaphor_manager::set_neighbour(const uint16_t& target_sem, const boost::container::vector<uint16_t>& n_id)
{
	for (uint8_t a = 0; a < static_cast<uint8_t>(n_id.size()); ++a)
	{
		//qDebug() << "[MANAGER]Target id =" << target_sem << "Neighbour id = " << n_id.at(a);
		s_list[target_sem]->add_neighbour(std::move(n_id.at(a)));
	}
}

//Запрашиваем состояние у светофора и возвращаем ответ
uint8_t semaphor_manager::get_neighbour_state(const uint16_t& n_id)
{
	return s_list[n_id]->get_reg_state();
}

//Запускаем генератор очередей в отдельном потоке
void semaphor_manager::run_queue_generator(const bool& mode)
{
	if (mode)
	{
		generator_mode = 1;
		generator_thread = new boost::thread(&semaphor_manager::queueGenerator, this);
		generator_thread->detach();
	}
}

//Функция генератора
void semaphor_manager::queueGenerator()
{
	//QString logout;
	uint16_t semaphor_id = 0;
	uint8_t cycle_cnt = 0;
	while (1)	//Бесконечный цикл удерживает поток в работе до поднятия флага stop_thread или generator_mode
	{
		//Костыль для регулировки частоты генерации, правое значение задаётся пользователем
		if (cycle_cnt == queue_delay)
		{
			semaphor_id = rand() % static_cast<uint16_t>(s_list.size()) + 0;	//Отправляем +1 к рандомному светофору
			addSemaphorQueue(semaphor_id);
			cycle_cnt = 0;
		}
		//Вырубаем цикл если приложение закрывается
		if (stop_thread)
			break;
		//qDebug() << "[EVENT QUEUE]Cycle" << cycle_cnt;
		//logout += "[GENERATOR THREAD] Added +1 queue to random semaphor(id=" + QString::number(semaphor_id) + ")";
		
		//Вырубаем цикл если пользователь останавливает генератор
		if (!generator_mode)
		{
			generator_mode = 1;
			break;
		}
		//qDebug() << logout;
		//logout.clear();
		++cycle_cnt;

		//Задержка
		boost::this_thread::sleep_for(boost::chrono::milliseconds(generator_timer_speed));
	}
}

//Запуск менеджера и перенос в отдельный поток
void semaphor_manager::run_manager()
{
	manager_thread = new boost::thread(&semaphor_manager::queueManager, this);
	manager_thread->detach();
}

//Менеджер
void semaphor_manager::queueManager()
{
	boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
	read_xml();
	uint8_t cycle_cnt = 0;
	while (1)
	{
		if (stop_thread)
		{
			semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[MANAGER]Stopping manager thread"));
			break;
		}
		//qDebug() << "[QUEUE]Cycle" << cycle_cnt;
		//Таймер запроса состояния всех подконтрольных светофоров

		//Раз в какое-то время(в циклах) идёт опрос всех светофоров и расчёт приоритетов
		if (cycle_cnt == manager_cycle)
		{
			calc_transit_priority();
			calc_udpate_cycle();
			cycle_cnt = 0;
		}

		++cycle_cnt;

		//Антилаг интерфейса
		QApplication::processEvents();

		boost::this_thread::sleep_for(boost::chrono::microseconds(manager_speed_cycle));
	}
}

/*
* Конченый метод
* 
* Производит расчет приоритета светофоров основываясь на информации полученной после запроса менеджера
* Определение разрешенных зон проводится только для светофора с 0м(наивысшим) приоритетом
* 
*/
void semaphor_manager::calc_transit_priority()
{
	boost::container::vector<uint16_t> _s_id = *s_id;
	boost::container::vector<uint16_t> _s_queue = s_queue;
	boost::container::vector<uint16_t>::iterator it;
	int zone_num = 0;
	//QString logout;
	uint16_t max_size_element = 0;
	uint16_t s_count = static_cast<uint16_t>(_s_id.size());
	//qDebug() << "[MANAGER]Calc transit order";
	queue_list->clear();
	while (1)
	{
		if (s_count == 0)
			break;
		//qDebug() << "[MANAGER]Calc cycle" << QString::number(s_count);
		it = std::max_element(_s_queue.begin(), _s_queue.end());	//Получаем указатель на элемент с максимальным размером
		//qDebug() << "[MANAGER]Max element find =" << QString::number(_s_queue.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))));

		//Записываем в карту с привязкой к ID светофора
		/*queue_list->push_back(
			std::pair<uint16_t, uint16_t>(static_cast<uint16_t>(s_id->size()) - s_count,
			_s_id.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))))
		);*/
		queue_list->insert(
			std::pair<uint16_t, uint16_t>(static_cast<uint16_t>(s_id->size())-s_count, 
			_s_id.at(static_cast<uint8_t>(std::distance(_s_queue.begin(), it))))
		);
		_s_queue[s_id->at(static_cast<int>(std::distance(_s_queue.begin(), it)))] = 0;	//Затираем значение чтобы не попадалось повторно
		--s_count;
	}
	//Логи бекенда
	//qDebug() << "[MANAGER]Print ordered list";
	//qDebug() << " _____________________________________________________________________________";
	for (auto it_map = queue_list->cbegin(); it_map != queue_list->cend(); ++it_map)
	{
		if (it_map->first == 0)	//Поиск принадлежности светофора с высшим приоритетом к соотв. зоне
			for (auto _it = semaphor_map->cbegin(); _it != semaphor_map->cend(); ++_it)
				for (uint8_t m_cnt = 0; m_cnt < _it->second.size(); ++m_cnt)
					if (it_map->second == _it->second[m_cnt])
						for (uint8_t z_lst_it = 0; z_lst_it < zone_list->size(); ++z_lst_it)
							for (uint8_t z_l_pos_it = 0; z_l_pos_it < zone_list->at(z_lst_it).size(); ++z_l_pos_it)
								if (_it->first == zone_list->at(z_lst_it).at(z_l_pos_it))
									n_zone = z_lst_it;
		//Логи фронтенда
		semaphor_gui::getInstance().write_table_content(it_map->first, 0, it_map->first);
		semaphor_gui::getInstance().write_table_content(it_map->first, 1, it_map->second);
		semaphor_gui::getInstance().write_table_content(it_map->first, 2, s_queue.at(it_map->second));
		//logout += "| Priority = " + QString::number(it_map->first).leftJustified(2, ' ')
		//	+ " | ID = " + QString::number(it_map->second).leftJustified(2, ' ')
		//	+ " | queue size = " + QString::number(s_queue.at(it_map->second)).leftJustified(2, ' ')
		//	+ " | zone ";	//Здесь надо было вкорячить лямбду но тот вариант который я нашёл роняет вижлу на дебаге
		//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Max queue size = " + QString::number(s_queue.at(it_map->second)).leftJustified(2, ' '));
		//Запись в таблицу и вывод лога в бекенд
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
						//logout += QString::number(zone_list->at(z_lst_it).at(z_sublst_it));
						semaphor_gui::getInstance().write_table_content(it_map->first, 3, zone_list->at(z_lst_it).at(z_sublst_it));
					}
				}
			}
		}
		//logout += " | cycle timer " + QString::number(s_list[it_map->second]->get_cycle_timer_value()).leftJustified(2, ' ') + " times |";
		semaphor_gui::getInstance().write_table_content(it_map->first, 4, s_list[it_map->second]->get_cycle_timer_value());
		//qDebug() << logout;
		//Чистим буфер лога
		//logout.clear();
	}
	//semaphor_gui::getInstance().slot_post_console_msg("[LOG]Stored zone list num" + QString::number(n_zone).leftJustified(2, ' '));
	//qDebug() << "[LOG]Stored zone list num" << QString::number(n_zone).leftJustified(2, ' ');
	//queue_list->clear();	//Не сюда
}

//Генератор запроса для светофора
uint8_t semaphor_manager::semaphor_request(const uint16_t& sem_id, const uint8_t& code)
{
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//Типа атомарность всё такое
	uint8_t response_code = 0x00;
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return 0;	//Если ничего такого нет то сразу выходим
	}
	//Если нужный светофор найден то блокируем другие потоки и формируем запрос
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);

	response_code = s_list[index]->get_reg_state();

	//qDebug() << "[LOG][semaphor_request]Semaphor id =" << sem_id << "returned response status=" << response_code;

	//m_mut.unlock();
	//Снимаем блокировку и выходим возвращая значение
	NOTIFY_ALL_THREAD
	//ul.unlock();

	return response_code;
}

//Генератор общего запроса(код рассылается всем светофорам разом)
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

//Сохраняет счетчик очереди светофора, вызвавшего этот метод
void semaphor_manager::load_s_queue(const uint16_t& sem_id, uint16_t queue_size)
{
	//qDebug() << "[MANAGER]Semaphor id =" << QString::number(sem_id) << " queue size =" << QString::number(queue_size);
	//THREAD_LOCK
	//std::unique_lock<std::mutex> ul(m_mut);
	//m_mut.lock();	//Типа атомарность всё такое
	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		//NOTIFY_ALL_THREAD
		//ul.unlock();
		return;	//Если ничего такого нет то сразу выходим
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);

	s_queue[index] = queue_size;
	//qDebug() << "[LOG][load_s_queue]Save queue size =" << s_queue[index] << "from semaphor id =" << sem_id;

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

//Аналогично запросу выше
bool semaphor_manager::send_command_to_target_semaphor(const uint16_t &sem_id, const uint8_t& code)
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
		return 0;	//Если ничего такого нет то сразу выходим
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);
	s_list[index]->manager_query_code = std::move(code);	//C26478 это баг!

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
	return 1;
}

//Задать имя светофору(имя показывается в логах бекенда)
void semaphor_manager::set_semaphor_name(const uint16_t &sem_id, const std::string& name)
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
		return;	//Если ничего такого нет то сразу выходим
	}
	THREAD_LOCK
	uint16_t index = std::distance(s_id->begin(), it);
	s_list[index]->set_name(std::move(name));

	//m_mut.unlock();
	NOTIFY_ALL_THREAD
	//ul.unlock();
}

//Запросить разрешение на проезд(отсылается светофором)
bool semaphor_manager::query_transit(const uint16_t& sender_id)
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

//Добавить светофор в зону
void semaphor_manager::add_semaphor_to_map(const boost::container::vector<uint16_t>& map)
{
	semaphor_map->insert(std::pair(semaphors_map_size, std::move(map)));
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[LOG]Import semaphor list"));
	//qDebug() << "[LOG]Import semaphor list";
	for (uint8_t a = 0; a < static_cast<uint8_t>(map.size()); ++a)
	{
		semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[VECTOR]Semaphor id = ") + QString::number(std::move(map[a])).leftJustified(2, ' ') + QObject::tr(" zone = ") + QString::number(semaphors_map_size).leftJustified(2, ' '));
		//qDebug() << "[VECTOR]Semaphor id =" << QString::number(map[a]).leftJustified(2, ' ') << "zone =" << QString::number(semaphors_map_size).leftJustified(2, ' ');
	}
	++semaphors_map_size;
}

//Настройка списка одновременно разрешенных к проезду зон
void semaphor_manager::add_parallel_zones(const boost::container::vector<uint8_t>& zone_lst)
{
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[LOG]Add zone list"));
	//qDebug() << "[LOG]Add zone list";
	for (uint8_t a = 0; a < zone_lst.size(); ++a)
	{
		semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[IMPORT] import zone") + QString::number(std::move(zone_lst[a])));
		//qDebug() << "[IMPORT] import zone" << QString::number(zone_lst[a]);
	}
	zone_list->push_back(std::move(zone_lst));
}

//Настройка частоты опроса менеджера в зависимости от очереди автомобилей перед светофором
void semaphor_manager::set_polling_graph(const std::pair<uint8_t, uint8_t>& section)
{
	//qDebug() << "[LOG]Add section {" << QString::number(section.first) << QString::number(section.second) << "}";
	polling_graph->push_back(std::move(std::pair<uint8_t,uint8_t>(section.first,section.second)));
}

//Рассчет цикла опроса светофоров исходя из зависимости "колво машин <-> частота опроса"
void semaphor_manager::calc_udpate_cycle()
{
	uint16_t max_queue = 0;
	//qDebug() << "[LOG]Calc update cycle, current cycle" << QString::number(manager_cycle) << "times";
	for (auto it_map = queue_list->cbegin(); it_map != queue_list->cend(); ++it_map)
	{
		if (max_queue < s_queue.at(it_map->second))
			max_queue = s_queue.at(it_map->second);
	}
	//qDebug() << "[LOG]Max queue =" << QString::number(max_queue);

	//По умолчанию ставим самый быстрый цикл
	manager_cycle = polling_graph->at(polling_graph->size() - 1).second;
	for (uint8_t a = polling_graph->size()-1; a > 0; --a)
	{
		//qDebug() << "[LOG]Cycle length" << QString::number(cycle_length) << QString::number(polling_graph->at(a).first) << QString::number(max_queue);
		if (polling_graph->at(a).first <= max_queue)
		{
			manager_cycle = polling_graph->at(a).second;
			//qDebug() << "[LOG]Set cycle length" << QString::number(manager_cycle);
			break;
		}
		else if (max_queue == 0)
		{
			manager_cycle = polling_graph->at(0).second;
			break;
		}
	}
	//qDebug() << "[LOG]Cycle length" << QString::number(cycle_length);
}

//Скопировать график зависимостей частоты от машин заданному светофору
void semaphor_manager::copy_polling_graph(const uint16_t& sem_id)
{
	THREAD_LOCK

	auto it = std::find(s_id->begin(), s_id->end(), sem_id);	//auto == uint16_t
	if (it == s_id->end())
	{
		//m_mut.unlock();
		NOTIFY_ALL_THREAD
		//ul.unlock();
		return;	//Если ничего такого нет то сразу выходим
	}
	uint16_t index = std::distance(s_id->begin(), it);

	for (uint8_t a = 0; a < polling_graph->size(); ++a)
	{
		s_list[sem_id]->set_polling_graph(polling_graph->at(a));
	}

	NOTIFY_ALL_THREAD
}

//Остановить все потоки светофоров
void semaphor_manager::stop_all_threads()
{
	for (uint8_t a = 0; a < s_list.size(); ++a)
	{
		s_list.at(a)->stop_thread = 1;
		//semaphor_gui::getInstance().slot_post_console_msg("[MANAGER]Stopping semaphor id = " + QString::number(a));
	}
	stop_thread = 1;
}

//Задать частоту генерации
void semaphor_manager::set_gen_freq(const int& freq)
{
	queue_delay = 167 - std::move(freq); //167 - минимальная частота, 0 - максимальная
}

//Остановка генератора
void semaphor_manager::stop_generator()
{
	generator_mode = 0;
}

void semaphor_manager::set_semaphor_speed(const uint16_t& speed)
{
	semaphor_speed = semaphor_timer_speed+std::move(speed);
	semaphor_request(SET_CYCLE_SPEED);
}

QDomElement make_element(QDomDocument& d_doc, const QString& str_name, const QString& str_attr = QString(), const QString& str_text = QString())
{
	QDomElement d_elem = d_doc.createElement(str_name);
	if (!str_attr.isEmpty())
	{
		QDomAttr d_attr = d_doc.createAttribute("attr");
		d_attr.setValue(str_attr);
		d_elem.setAttributeNode(d_attr);
	}
	if (!str_text.isEmpty())
	{
		QDomText d_txt = d_doc.createTextNode(str_text);
		d_elem.appendChild(d_txt);
	}
	return d_elem;
}

QDomElement semaphor_manager::parametr(QDomDocument& d_doc, const QString& param_name, const QString& param_value)
{
	QDomElement dom_element = make_element(d_doc, "Settings", "uint16_t");
	dom_element.appendChild(make_element(d_doc, "param", "", param_name));
	dom_element.appendChild(make_element(d_doc, "value", "", param_value));
	
	return dom_element;
}

uint16_t semaphor_manager::get_cycle_timer_speed_ms()
{
	return semaphor_timer_speed;
}

uint8_t semaphor_manager::read_update_interval_period()
{
	return gui_refresh_speed_cycle;
}

void semaphor_manager::read_xml()
{
	xml_set_file = new QFile("param.xml");
	if (xml_set_file->open(QIODevice::ReadOnly))
	{
		semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Load XML"));
		//qDebug() << "Read param";
		QXmlStreamReader xml_set(xml_set_file);
		while (!xml_set.atEnd() && !xml_set.hasError())
		{
			QXmlStreamReader::TokenType token = xml_set.readNext();
			//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
			if (xml_set.name() == "__Shani_basic")
			{
				semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Setting file signature [") + xml_set.name() + QObject::tr("] VALID"));
				//qDebug() << "Settings signature" << xml_set.name() << "VALID";
				while (!xml_set.atEnd() && !xml_set.hasError())
				{
					//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
					QXmlStreamReader::TokenType token = xml_set.readNext();
					if (token == QXmlStreamReader::StartDocument)
						continue;
					if (token == QXmlStreamReader::Characters)
					{
						//qDebug() << xml_set.name() << xml_set.text();
						if (xml_set.text() == "semaphor_speed_cycle")
						{
							while (xml_set.name() != "Settings")
							{
								//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
								if (token == QXmlStreamReader::StartElement)
								{
									token = xml_set.readNext();
									//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
									semaphor_timer_speed = xml_set.text().toInt();
									semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Set semaphor thread speed ") + QString::number(semaphor_timer_speed) + QObject::tr(" ms"));
									//qDebug() << QString::number(semaphor_timer_speed);
									semaphor_request(SET_CYCLE_MS);
								}
								token = xml_set.readNext();
							}
						}
						else if (xml_set.text() == "generator_speed_cycle")
						{
							while (xml_set.name() != "Settings")
							{
								//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
								if (token == QXmlStreamReader::StartElement)
								{
									token = xml_set.readNext();
									//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
									generator_timer_speed = xml_set.text().toInt();
									semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Set generator speed ") + QString::number(generator_timer_speed) + QObject::tr(" ms"));
									//qDebug() << QString::number(generator_timer_speed);
								}
								token = xml_set.readNext();
							}
							//qDebug() << xml_set.text();
						}
						else if (xml_set.text() == "manager_speed_cycle")
						{
							while (xml_set.name() != "Settings")
							{
								//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
								if (token == QXmlStreamReader::StartElement)
								{
									token = xml_set.readNext();
									//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
									manager_speed_cycle = xml_set.text().toInt();
									semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Set manager cycle ") + QString::number(manager_speed_cycle) + QObject::tr(" ms"));
									//qDebug() << QString::number(manager_speed_cycle);
								}
								token = xml_set.readNext();
							}
							//qDebug() << xml_set.text();
						}
						else if (xml_set.text() == "gui_refresh_cycle")
						{
							//q
							while (xml_set.name() != "Settings")
							{
								//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
								if (token == QXmlStreamReader::StartElement)
								{
									token = xml_set.readNext();
									//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
									gui_refresh_speed_cycle = xml_set.text().toInt();
									semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Set GUI refresh cycle ") + QString::number(gui_refresh_speed_cycle) + QObject::tr(" ms"));
									//qDebug() << QString::number(gui_refresh_speed_cycle);
									semaphor_gui::getInstance().set_refresh_interval_ms(gui_refresh_speed_cycle);
								}
								token = xml_set.readNext();
							}
							//Debug() << xml_set.text();
						}
						else if (xml_set.text() == "gui_refresh_interval")	//Не используется
						{
							while (xml_set.name() != "Settings")
							{
								//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
								if (token == QXmlStreamReader::StartElement)
								{
									token = xml_set.readNext();
									//qDebug() << xml_set.tokenString() << xml_set.name() << xml_set.text();
									gui_refresh_interval_cycle = xml_set.text().toInt();
									semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[XML PARSER]Unused uint16_t=") + QString::number(gui_refresh_interval_cycle) + "");
									//qDebug() << QString::number(gui_refresh_interval_cycle);
								}
								token = xml_set.readNext();
							}
							//qDebug() << xml_set.text();
							break;	//Вырубаем цикл так как всё прочитано уже
						}
					}
				}
			}
		}
		xml_set_file->close();
	}
	else //Если файла не существует то создаем и сохраняем настройк по умолчанию
	{
		semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[MANAGER]XML file not found. Making new"));
		xml_set_document = new QDomDocument("__Shani_basic");;
		QDomElement xml_set_element = xml_set_document->createElement("__Shani_basic");
		xml_set_document->appendChild(xml_set_element);
		xml_set_file->open(QIODevice::WriteOnly);
		semaphor_speed_cycles = new QDomElement(parametr(*xml_set_document, "semaphor_speed_cycle", QString::number(semaphor_timer_speed)));
		generator_speed_cycles = new QDomElement(parametr(*xml_set_document, "generator_speed_cycle", QString::number(generator_timer_speed)));
		manager_speed_cycles = new QDomElement(parametr(*xml_set_document, "manager_speed_cycle", QString::number(manager_speed_cycle)));
		gui_refresh_speed_cycles = new QDomElement(parametr(*xml_set_document, "gui_refresh_cycle", QString::number(gui_refresh_speed_cycle)));
		gui_refresh_interval = new QDomElement(parametr(*xml_set_document, "gui_refresh_interval", QString::number(gui_refresh_interval_cycle)));
		xml_set_element.appendChild(*semaphor_speed_cycles);
		xml_set_element.appendChild(*generator_speed_cycles);
		xml_set_element.appendChild(*manager_speed_cycles);
		xml_set_element.appendChild(*gui_refresh_speed_cycles);
		xml_set_element.appendChild(*gui_refresh_interval);
		QTextStream(xml_set_file) << xml_set_document->toString();
		xml_set_file->close();
	}
}

void semaphor_manager::run_all_semaphors()
{
	for (uint8_t a = 0; a < s_list.size(); ++a)
	{
		//Снимаем все потоки с паузы
		s_list[a]->thread_wait = 0;
	}
}
