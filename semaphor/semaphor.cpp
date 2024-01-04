#include "semaphor.h"
#include "manager.h"
#include "semaphor_event.h"
#include "gui.h"
#include <qdebug.h>
#include <qstring.h>
#include <mutex>
#include <string>
#include "boost/thread/thread.hpp"
#include "boost/chrono/chrono.hpp"
#include "boost/container/vector.hpp"
#include <qobject.h>
std::mutex t_mut;

semaphor::semaphor()
{
	//Инициализация
	myName = "NULL";
	sThread = NULL;
	myId = 0x0000;
	status = RED;
	roadscanTimer = 0;
	transitionAllowTimer = 0;
	waitingTimer = 0;
	manager_query_code = 0x00;
	transitAllowed = 0;
	queue = 0;
	roadBusy = 0;
	cycle_timer = 0;
	stop_thread = 0;
	cycle_speed = 1;
	thread_wait = 1;	//Светофор запускается и сразу в паузу
	neighbour = new boost::container::vector<uint16_t>;
	polling_graph = new boost::container::vector<std::pair<uint8_t, uint8_t>>;
}
semaphor::~semaphor()
{
	if (sThread->joinable())
		sThread->join();
}

//Не используется
void semaphor::test_hello()
{
	semaphor_manager::getInstance().add_semaphor(myId);
}

//Создать поток и запустить
void semaphor::new_thread(const uint16_t& a)
{
	myId = std::move(a);
	//qDebug() << "[THREAD]Creating a new semaphore-thread with id =" << myId;
	sThread = new boost::thread(&semaphor::run_thread, this);
	if (sThread->joinable())
		sThread->detach();
}

//Добавить соседа в список зависимостей(см описание в листинге менеджера)
void semaphor::add_neighbour(const uint16_t& nb_id)
{
	//qDebug() << "[THREAD]Adding new neighbour id =" << nb_id;
	neighbour->push_back(std::move(nb_id));
	neighbout_reg_state.push_back(0x00);
}

//Получить регистр состояния
uint8_t semaphor::get_reg_state()
{
	return status;
}

/*
* Опрос соседей через менеджера
* 
* Светофор отправляет запрос менеджеру с указанием ИД светофора, полученного в методе add_neighbour
* 
* Менеджер ищет светофор у себя и в случае успеха отправляет запрос. Далее ответ через менеджера направляются
* Запросившему светофору
*/
void semaphor::request_neighbour_status()
{
	//QString logout;
	t_mut.lock();
	for (uint8_t a = 0; a < static_cast<uint8_t>(neighbout_reg_state.size()); ++a)
	{
		neighbout_reg_state.at(a) = semaphor_manager::getInstance().semaphor_request(neighbour->at(a),0x01);
		//logout += "[THREAD]Requested semaphore id=" + QString::number(neighbour->at(a)) + " return response status ";
		switch (neighbout_reg_state.at(a))
		{
			case 0x00:
			{
				//logout += "NULL";
				break;
			}
			case RED:
			{
				//logout += "RED";
				break;
			}
			case YELLOW:
			{
				//logout += "YELLOW";
				break;
			}
			case GREEN:
			{
				//logout += "GREEN";
				break;
			}
		}
		//qDebug() << logout;
		//logout.clear();
	}
	t_mut.unlock();
}

/*
* Проверка дороги. Если все соседи светофора запретили движение то считается
* что дорога пуста и светофор направляет запрос менджеру на разрешение пропуска
* транспорта
*/
bool semaphor::road_safety_check()
{
	//if(myId == 0x0009 || myId == 0x000A || myId == 0x000B)
	//	qDebug() << "[THREAD" << QString::number(myId) << "]Check road";
	bool accepted = 1;
	for (uint8_t a = 0; a < static_cast<uint8_t>(neighbour->size()); ++a)
	{
		if (neighbout_reg_state[a] == RED)
		{
			//if (myId == 0x0009 || myId == 0x000A || myId == 0x000B)
			//	qDebug() << "[THREAD]Semaphor id=" << QString::number(myId) << "Safety check: TRANSIT ALLOWED with status" << QString::number(neighbout_reg_state[a]);
			accepted = 1;
		}
		else
		{
			//if (myId == 0x0009 || myId == 0x000A || myId == 0x000B)
			//	qDebug() << "[THREAD]Semaphor id=" << QString::number(myId) << "Safety check: TRANSIT REJECTED with status" << QString::number(neighbout_reg_state[a]);
			return 0;
		}
	}
	return accepted;
}

//Пропуск автомобиля
void semaphor::car_pass()
{
	//QString logout; //Логи в бекенд
	if (queue > 0 && transitAllowed == 1)
	{
		//logout += "[THREAD][ " + QString::fromStdString(myName) + " ][ID=" + QString::number(myId).leftJustified(2, ' ') +"][    PASS    ][Queue size=" + QString::number(queue).leftJustified(3, ' ') + "][neighbours status:";
		for (uint8_t a = 0; a < static_cast<uint8_t>(neighbour->size()); ++a)
		{
			switch (neighbout_reg_state[a])
			{
				case NULL:
				{
					//logout += " NULL ";
					break;
				}
				case RED:
				{
					//logout += " RED  ";
					break;
				}
				case YELLOW:
				{
					//logout += " YELLOW";
					break;
				}
				case GREEN:
				{
					//logout += " GREEN";
					break;
				}
			}
		}
		//logout += "]";
		//qDebug() << "[THREAD]Semaphor id=" << QString::number(myId).leftJustified(2, ' ') << "queue size =" << QString::number(queue).leftJustified(4, ' ') << "PASS";
		transitAllowed = 0;
		--queue;
	}
	else
	{
		//logout += "[THREAD][ " + QString::fromStdString(myName) + " ][ID=" + QString::number(myId).leftJustified(2, ' ') + "][ NO TRANSIT ][Queue size=" + QString::number(queue).leftJustified(3, ' ') + "][neighbours status:";
		for (uint8_t a = 0; a < static_cast<uint8_t>(neighbour->size()); ++a)
		{
			switch (neighbout_reg_state[a])
			{
				case NULL:
				{
					//logout += " NULL ";
					break;
				}
				case RED:
				{
					//logout += " RED  ";
					break;
				}
				case YELLOW:
				{
					//logout += " YELLOW";
					break;
				}
				case GREEN:
				{
					//logout += " GREEN";
					break;
				}
			}
		}
		//logout += "]";
		//qDebug() << "[THREAD]Semaphor id=" << QString::number(myId) << "queue size =" << QString::number(queue).leftJustified(4, ' ') << "NO TRANSIT";
	}
	//qDebug() << logout;
}

//Задать имя светофору(показывается в логах бекенда только)
void semaphor::set_name(const std::string& name)
{
	myName = std::move(name);
}

//Читает команду от менеджера(поле volatile) и выполняет
void semaphor::read_manager_command()
{
	//QString logout;
	switch (manager_query_code)
	{
		case 0x00:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " no query]";
			//qDebug() << logout;
			break;
		}
		case 0x01:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " status request]";
			semaphor_manager::getInstance().load_s_state(myId, status);
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0x02:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " transit allowed]";
			transitAllowed = 1;
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0x03:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " transit disallowed]";
			transitAllowed = 0;
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0x04:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " setup shot timer]";
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0xFF:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " disable semaphor]";
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0x05:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " setup send query timer]";
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
		case 0x06:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " queue count]";
			semaphor_manager::getInstance().load_s_queue(myId, queue);
			//qDebug() << logout;
			manager_query_code = 0x00;
			break;
		}
		case 0x07:
		{
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " set cycle speed " + QString::number(semaphor_manager::getInstance().semaphor_speed) + "+ms]";
			cycle_speed = semaphor_manager::getInstance().semaphor_speed;
			//qDebug() << logout;
			manager_query_code = 0x00;
			break;
		}
		case 0x08:
		{
			cycle_speed = semaphor_manager::getInstance().get_cycle_timer_speed_ms();
			//logout += "[query code " + QString::number(manager_query_code).leftJustified(2, ' ') + " set cycle speed " + QString::number(cycle_speed) + "+ms]";
			manager_query_code = 0x00;
			//qDebug() << logout;
			break;
		}
	}
	//logout.clear();
}

//Скопировать график себе посекционно
void semaphor::set_polling_graph(const std::pair<uint8_t, uint8_t>& section)
{
	//qDebug() << "[THREAD, Semaphor id" << QString::number(myId) << "]Add section{" << QString::number(section.first) << QString::number(section.second) << "}";
	polling_graph->push_back(std::move(std::pair<uint8_t, uint8_t>(section.first, section.second)));
}

//Расчет частоты опроса менеджера. Работает аналогично расчету в менеджере
void semaphor::calc_query_cycle()
{
	//qDebug() << "[THREAD, Semaphor id" << QString::number(myId) << "]Calc cycle, current" << QString::number(cycle_timer) << "times";
	// 
	//По умолчанию ставим самый быстрый цикл
	cycle_timer = polling_graph->at(polling_graph->size() - 1).second;
	for (uint8_t a = polling_graph->size() - 1; a > 0; --a)
	{
		//qDebug() << "[LOG]Cycle length" << QString::number(cycle_length) << QString::number(polling_graph->at(a).first) << QString::number(max_queue);
		if (polling_graph->at(a).first <= queue)
		{
			cycle_timer = polling_graph->at(a).second;
			//qDebug() << "[LOG]Set cycle length" << QString::number(manager_cycle);
			break;
		}
		else if (queue == 0)
		{
			cycle_timer = polling_graph->at(0).second;
			break;
		}
	}
	//qDebug() << "[THREAD, Semaphor id" << QString::number(myId) << "]Set cycle length" << QString::number(cycle_timer) << "times";
}

//Получить текущее состояние таймера светофора
uint8_t semaphor::get_cycle_timer_value()
{
	return cycle_timer;
}

//Основной поток светофора
void semaphor::run_thread()
{
	//QString logout;
	uint16_t cycle_cnt = 0;
	uint16_t timer = 0;

	//Задержка запуска, для того чтобы всё зависящее успело инициализироваться

	//thread_wait = 1;

	if (thread_wait)
	{
		while (thread_wait)
		{
			//NOP ждём, т.к пауза цикла
		}
	}

	//boost::this_thread::sleep_for(boost::chrono::milliseconds(1927));
	semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[THREAD]Run thread id=") + QString::number(0));

	while (1)
	{
		//qDebug() << "[THREAD" << QString::number(myId) << "]Cycle" << cycle_cnt;

		boost::this_thread::sleep_for(boost::chrono::milliseconds(cycle_speed));
		//Читаем команду полученную от менеджера
		read_manager_command();

		if (stop_thread)
		{
			//semaphor_gui::getInstance().slot_post_console_msg("[THREAD ID = " + QString::number(myId) + "]Stop thread");
			break;
		}

		//Передаем менеджеру текущую очередь
		//Опрашиваем соседей
		if (timer == cycle_timer*2)
		{
			semaphor_manager::getInstance().load_s_queue(myId, queue);
			request_neighbour_status();
			calc_query_cycle();
			timer = 0;
		}

		semaphor_gui::getInstance().write_queue_cnt(myId, queue);

		//Запрашиваем у менеджера состояние соседей
		for (uint8_t a = 0; a < static_cast<uint8_t>(neighbour->size()); ++a)
		{
			switch (neighbout_reg_state[a])
			{
				case 0x00:
				{
					//logout += "[THREAD, semaphor id=" + QString::number(myId).leftJustified(2, ' ') + "]Neighbour id=" + QString::number(neighbour->at(a)).leftJustified(2, ' ') + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))).leftJustified(2, ' ') + " DISABLED";
					//logout += "[THREAD ID=" + QString::number(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ", semaphor id=" + QString::number(myId) + "]Neighbour id=" + QString::number(neighbour->at(a)) + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))) + " DISABLED";
					//qDebug() << logout;
					//logout.clear();
					break;
				}
				case 0x01:
				{
					//logout += "[THREAD, semaphor id=" + QString::number(myId).leftJustified(2, ' ') + "]Neighbour id=" + QString::number(neighbour->at(a)).leftJustified(2, ' ') + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))).leftJustified(2, ' ') + " RED";
					//logout += "[THREAD ID=" + QString::number(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ", semaphor id=" + QString::number(myId) + "]Neighbour id=" + QString::number(neighbour->at(a)) + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))) + " RED";
					//qDebug() << logout;
					//logout.clear();
					break;
				}
				case 0x02:
				{
					//logout += "[THREAD, semaphor id=" + QString::number(myId).leftJustified(2, ' ') + "]Neighbour id=" + QString::number(neighbour->at(a)).leftJustified(2, ' ') + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))).leftJustified(2, ' ') + " YELLOW";
					//logout += "[THREAD ID=" + QString::number(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ", semaphor id=" + QString::number(myId) + "]Neighbour id=" + QString::number(neighbour->at(a)) + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))) + " YELLOW";
					//qDebug() << logout;
					//logout.clear();
					break;
				}
				case 0x04:
				{
					//logout += "[THREAD, semaphor id=" + QString::number(myId).leftJustified(2, ' ') + "]Neighbour id=" + QString::number(neighbour->at(a)).leftJustified(2, ' ') + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))).leftJustified(2, ' ') + " GREEN";
					//logout += "[THREAD ID=" + QString::number(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ", semaphor id=" + QString::number(myId) + "]Neighbour id=" + QString::number(neighbour->at(a)) + " has state " + QString::number(semaphor_manager::getInstance().get_neighbour_state(neighbour->at(a))) + " GREEN";
					//qDebug() << logout;
					//logout.clear();
					break;
				}
			}
			//qDebug() << logout;
			//logout.clear();
		}

		//Проверяем пропускает ли какой нибудь сосед машины
		if (road_safety_check()&&queue>0)
		{
			//Запрашиваем разрешение на транзит
			if (semaphor_manager::getInstance().query_transit(myId))
			{
				//qDebug() << "[THREAD, semaphor id =" << QString::number(myId).leftJustified(2, ' ') << "] Transit";
				transitAllowed = 1;
				status = GREEN;
				semaphor_gui::getInstance().set_signal(myId, transitAllowed);
				car_pass();
			}
			else
			{
				//qDebug() << "[THREAD, semaphor id =" << QString::number(myId).leftJustified(2, ' ') << "] Wait";
				transitAllowed = 0;
				status = RED;
				semaphor_gui::getInstance().set_signal(myId, transitAllowed);
				car_pass();
			}
		}
		else
		{
			transitAllowed = 0;
			status = RED;
			semaphor_gui::getInstance().set_signal(myId, transitAllowed);
			car_pass();
		}

		//Костыль
		//state = semaphor_manager::getInstance().read_s_state(myId);

		/*logout += "[THREAD]Semaphor id = " + QString::number(myId) + " queue size " + QString::number(queue) + " ";
		if (queue > 0 && transitAllowed)
		{
			qDebug() << "[THREAD]Semaphor id=" << QString::number(myId) << "PASS";
			logout += "[transit allowed ]";
			transitAllowed = 0;
			--queue;
		}
		else
		{
			qDebug() << "[THREAD]Semaphor id=" << QString::number(myId) << "NO TRANSIT";
			logout += "[transit disallowed ]";
		}*/
		++timer;
		++cycle_cnt;
	}
}