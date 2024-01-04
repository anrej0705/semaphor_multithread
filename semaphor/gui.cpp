#include "gui.h"
#include "manager.h"
#include "boost/thread/thread.hpp"
#include <mutex>
#include <condition_variable>
std::condition_variable condition_gui;
std::mutex m_mut_gui;

//Эту дичь пихать только в методы
#define THREAD_LOCK std::lock_guard<std::mutex> lg_gui(m_mut_gui);
#define NOTIFY_ALL_THREAD condition_gui.notify_all();

semaphor_gui* semaphor_gui::s_slot_ptr = 0;
singlet_destroyer2 semaphor_gui::dstr;

/*/
/Класс, гарантирующий удаление после выхода из проги
/*/
singlet_destroyer2::~singlet_destroyer2()
{
	if (m_inst != NULL)
		delete m_inst;
}
void singlet_destroyer2::initialize(semaphor_gui* ptr)
{
	m_inst = ptr;
}

/*
* Конструктор
* 
* Настройка интерфейса и инициализация переменных
*/
semaphor_gui::semaphor_gui(QWidget* qwgt) : QWidget(qwgt)
{
	//Инициализации
	background_img = new QLabel;
	hLay = new QHBoxLayout;
	background_pxmap = new QPixmap("cross.png");
	control_lay = new QVBoxLayout;
	control_panel = new QWidget;
	generator_module = new QGroupBox(tr("Queue generator mode"));
	state_machine_lay = new QHBoxLayout;
	slider_lay = new QHBoxLayout;
	generator_lay = new QVBoxLayout;
	gen_rate_slider = new QSlider(Qt::Horizontal);
	generator_mode = new QStateMachine;
	mode_en = new QState(generator_mode);
	mode_dis = new QState(generator_mode);
	lbl_en = new QLabel(tr("Enable --->"));
	lbl_dis = new QLabel(tr("<--- Disable"));
	mode_switcher = new QPushButton(tr("Switch"), generator_module);
	console_lay = new QHBoxLayout;
	slider_indicator = new QLabel(QString::number(25));
	lbl_frequency = new QLabel(tr("Frequency: "));
	semaphor_speed_slider = new QSlider(Qt::Horizontal);
	semaphor_module_lay = new QVBoxLayout;
	semaphor_speed_slider_box = new QGroupBox(QObject::tr("Semaphor speed in cycles"));
	s_speed_lbl = new QLabel(QObject::tr("Speed"));
	speed_indc = new QLabel("32");
	s_speed_lay = new QHBoxLayout;
	slot_cnt = 0;
	thread_stop = 0;
	gen_mode = 0;
	s_g_cnt = 0;

	refresh_interval = 125;	//Период обновления графики

	semaphor_manager::getInstance().set_gen_freq(167-25);

	this->setWindowTitle("Semaphors v1.1a");

	semaphor_slots = new boost::container::vector<semaphor_slot*>;
	//semaphor_graphic_arr = new boost::container::vector<semaphor_graphic*>;

	//Настройка модуля генератора
	slider_indicator->setFixedWidth(24);
	gen_rate_slider->setRange(1, 166);
	gen_rate_slider->setTickInterval(4);
	gen_rate_slider->setValue(25);
	gen_rate_slider->setTickPosition(QSlider::TicksBelow);
	generator_module->setLayout(generator_lay);
	//generator_module->setFixedWidth(900);
	generator_module->setFixedHeight(112);
	state_machine_lay->addWidget(lbl_en);
	state_machine_lay->addStretch(1);
	state_machine_lay->addWidget(lbl_dis);
	mode_switcher->setAutoFillBackground(1);
	mode_switcher->show();
	slider_lay->addWidget(lbl_frequency);
	slider_lay->addWidget(gen_rate_slider);
	slider_lay->addWidget(slider_indicator);
	int nBtnWidth = generator_module->width();

	//Параметры кнопки подобраны чтобы она вписывалась в рамку
	QRect rect1(10, 66, nBtnWidth/2, generator_module->height()-76);
	mode_dis->assignProperty(mode_switcher, "geometry", rect1);
	mode_dis->assignProperty(lbl_dis, "visible", 1);
	mode_dis->assignProperty(lbl_en, "visible", 0);
	generator_mode->setInitialState(mode_dis);

	//Параметры кнопки подобраны чтобы она вписывалась в рамку
	QRect rect2(nBtnWidth/2+146, 66, nBtnWidth/2-10, generator_module->height()-76);
	mode_en->assignProperty(mode_switcher, "geometry", rect2);
	mode_en->assignProperty(lbl_dis, "visible", 0);
	mode_en->assignProperty(lbl_en, "visible", 1);
	QSignalTransition* p_trans1 = mode_dis->addTransition(mode_switcher, SIGNAL(clicked()), mode_en);
	QSignalTransition* p_trans2 = mode_en->addTransition(mode_switcher, SIGNAL(clicked()), mode_dis);
	QPropertyAnimation* p_anim1 = new QPropertyAnimation(mode_switcher, "geometry");
	p_trans1->addAnimation(p_anim1);
	QPropertyAnimation* p_anim2 = new QPropertyAnimation(mode_switcher, "geometry");
	p_trans2->addAnimation(p_anim2);
	generator_lay->addLayout(slider_lay);
	generator_lay->addLayout(state_machine_lay);
	generator_mode->start();

	//Подключаем машину состояний и прочие зависимости
	if (!QObject::connect(gen_rate_slider, SIGNAL(valueChanged(int)), slider_indicator, SLOT(setNum(int))));
		//QErrorMessage::qtHandler();
	if (!QObject::connect(gen_rate_slider, SIGNAL(valueChanged(int)), this, SLOT(set_generator_frequency(int))));
		//QErrorMessage::qtHandler();
	if (!QObject::connect(mode_en, &QState::entered, this, &semaphor_gui::switch_generator_mode));
		//QErrorMessage::qtHandler();
	if (!QObject::connect(mode_en, &QState::exited, this, &semaphor_gui::switch_generator_mode));
		//QErrorMessage::qtHandler();
	if (QObject::connect(semaphor_speed_slider, SIGNAL(valueChanged(int)), speed_indc, SLOT(setNum(int))));
		//QErrorMessage::qtHandler();
	if (QObject::connect(semaphor_speed_slider, SIGNAL(valueChanged(int)), this, SLOT(set_semaphor_speed(int))));
		//QErrorMessage::qtHandler();
	

	//Настройка панели со слотами-светофорами
	slots_module = new QGroupBox(tr("Semaphors"));
	slots_module_lay = new QHBoxLayout;

	/*for (uint8_t a = 0; a < 12; ++a)
	{
		semaphor_slots[a] = new semaphor_slot;
		semaphor_slots[a]->slot_set_id(a+1);
		slots_module_lay->addWidget(semaphor_slots[a]);
	}*/
	semaphor_speed_slider->setRange(1, 2048);
	semaphor_speed_slider->setValue(32);
	semaphor_speed_slider->setTickInterval(48);
	semaphor_speed_slider->setTickPosition(QSlider::TicksBelow);
	s_speed_lay->addWidget(s_speed_lbl);
	s_speed_lay->addWidget(semaphor_speed_slider);
	s_speed_lay->addWidget(speed_indc);
	semaphor_speed_slider_box->setLayout(s_speed_lay);
	semaphor_module_lay->addWidget(semaphor_speed_slider_box);
	semaphor_module_lay->addLayout(slots_module_lay);
	slots_module->setLayout(semaphor_module_lay);
	slots_module->setFixedHeight(420);
	speed_indc->setFixedWidth(24);

	//Настройка консоли
	console_box = new QGroupBox(tr("Console"));
	console_box_lay = new QVBoxLayout;
	console = new QTextEdit;
	console->setReadOnly(1);
	console_box_lay->addWidget(console);

	//Настройка табляцы
	table_box = new QGroupBox(tr("Manager info"));
	table_box_lay = new QVBoxLayout;
	table = new QStandardItemModel(12, 5);
	table->setHorizontalHeaderItem(0, new QStandardItem(tr("Priority")));
	table->setHorizontalHeaderItem(1, new QStandardItem(tr("ID")));
	table->setHorizontalHeaderItem(2, new QStandardItem(tr("Queue")));
	table->setHorizontalHeaderItem(3, new QStandardItem(tr("Zone")));
	table->setHorizontalHeaderItem(4, new QStandardItem(tr("Timer(cycle)")));
	table_view = new QTreeView;
	table_view->setModel(table);
	table_view->header()->resizeSection(0, 65);
	table_view->header()->resizeSection(1, 46);
	table_view->header()->resizeSection(2, 55);
	//table_view->header()->resizeSection(3, 5);
	//table_view->header()->resizeSection(4, 5);
	table_box_lay->addWidget(table_view);
	table_box->setLayout(table_box_lay);

	//Компоновка таблицы и консоли
	console_box->setLayout(console_box_lay);
	console_lay->addWidget(table_box);
	console_lay->addWidget(console_box);

	//Финальная компоновка
	control_lay->addWidget(generator_module);
	control_lay->addWidget(slots_module);
	control_lay->addLayout(console_lay);

	//Настройка панели управления
	control_panel->setLayout(control_lay);
	//control_panel->resize(768, 768);

	background_img->setPixmap(background_pxmap->scaled(768,768,Qt::KeepAspectRatio,Qt::SmoothTransformation));

	//hLay->addWidget(plbl());
	hLay->addWidget(background_img);
	hLay->addWidget(control_panel);
	this->setLayout(hLay);
	this->move(60, 100);
	//this->resize(1600, 768);
	this->setFixedSize(1600, 791);
	for(uint8_t a=0;a<12;++a)
		s_graph[a] = new semaphor_graphic(this, 0, 0);
	//semaphor_graphic* red = new semaphor_graphic(this, 486, 288);

	//Загружаем прочитанное из xml значение
	refresh_interval = semaphor_manager::getInstance().read_update_interval_period();

	this->setWindowIcon(QPixmap("icon.ico"));
}

semaphor_gui::~semaphor_gui()
{}
semaphor_gui& semaphor_gui::getInstance()
{
	if (!s_slot_ptr)
	{
		s_slot_ptr = new semaphor_gui();
		dstr.initialize(s_slot_ptr);
	}
	return *s_slot_ptr;
}

/*
* Добавляет слот в виде панели управления и выполняет привязку по ИД
*/
void semaphor_gui::add_slot(uint16_t sem_id)
{
	semaphor_slots->push_back(new semaphor_slot);
	slots_module_lay->addWidget(semaphor_slots->at(slot_cnt));
	semaphor_slots->at(sem_id)->slot_set_id(sem_id);
	semaphor_state.resize(slot_cnt + 1);
	semaphor_gueue_cnt.resize(slot_cnt + 1);
	semaphor_state.push_back(std::pair<uint16_t, bool>(sem_id, 0));
	semaphor_gueue_cnt.push_back(std::pair<uint16_t, uint16_t>(sem_id, 0));
	++slot_cnt;
}

//Поток вызывает этот метод чтобы передать информацию о состоянии очереди
void semaphor_gui::write_queue_cnt(const uint16_t& sem_id, uint16_t queue_cnt)
{
	//THREAD_LOCK
	//qDebug() << "[GUI]Write queue size" << QString::number(queue_cnt) << "to panel id" << QString::number(sem_id);
	semaphor_slots->at(sem_id)->slot_set_queue(queue_cnt);
	semaphor_gueue_cnt.at(sem_id).second = queue_cnt;
	//NOTIFY_ALL_THREAD
}

//Записывает в таблицу информацию. Вызывается менеджером
void semaphor_gui::write_table_content(const uint8_t& section_num, const uint8_t& row_num, const uint16_t& arg)
{
	QModelIndex index = table->index(std::move(section_num), std::move(row_num));
	table->setData(index, std::move(arg), Qt::EditRole);
	//table->dataChanged(index, index);
}

//Запускает поток обслуживающий графический интерфейс
void semaphor_gui::run_gui()
{
	gui_thread = new boost::thread(&semaphor_gui::gui, this);
	gui_thread->detach();
}

//Обслуживание(по факту дёрганье) графического интерфейса
//Данный метод является костылём который убирает периодическое
//Подвисание интерфейса во время работы
void semaphor_gui::gui()
{
	QModelIndex index_bot = table->index(0, 0);
	QModelIndex index_top = table->index(11, 4);
	//QTimer timer;
	//timer.setInterval(0);
	while (1)
	{
		if (thread_stop)
		{
			//slot_post_console_msg("[GUI]Stop gui refresher");
			break;
		}
		//QTimer::singleShot(0, this, SLOT(showGUI()));
		//timer.start();
		QApplication::processEvents();
		table_view->dataChanged(index_bot, index_top);
		//QCoreApplication::processEvents();
		/*for (auto const& x : semaphor_state)
		{
			//qDebug() << QString::number(x.first) << QString::number(x.second);
			semaphor_slots->at(x.first)->slot_set_signal(x.second);
		}
		for (auto const &x : semaphor_gueue_cnt)
		{
			semaphor_slots->at(x.first)->slot_set_queue(x.second);
		}*/
		boost::this_thread::sleep_for(boost::chrono::milliseconds(refresh_interval));
	}
}

//Записывает флаг состояния светофора(красный или зелёный), вызывается потоком
void semaphor_gui::set_signal(const uint16_t& sem_id, bool signal)
{
	//Упрощенная реализация. В более сложной модели потребуется замена вектора на карту либо вектор + std::pair
	//THREAD_LOCK
	semaphor_slots->at(sem_id)->slot_set_signal(signal);
	QMetaObject::invokeMethod(s_graph[sem_id], [=]() {
		s_graph[sem_id]->set_state(signal);
		});
	//semaphor_state.at(sem_id).first = sem_id;
	//semaphor_state.at(sem_id).second = signal;
	//NOTIFY_ALL_THREAD
}

//Запись сообщения в консоль пользователя(не бекенд!)
void semaphor_gui::slot_post_console_msg(QString msg)
{
	//THREAD_LOCK
	console->append(msg);
	//console->append("\r");
	//NOTIFY_ALL_THREAD
}

//Отображение ГУЯ
void semaphor_gui::showGUI()
{
	//THREAD_LOCK
	this->show();
	//NOTIFY_ALL_THREAD
}

//Перехватчик события закрытия программы. Сначала останавливаются потоки, потом сама программа
void semaphor_gui::closeEvent(QCloseEvent* ce)
{
	slot_post_console_msg(QObject::tr("[LOG]Stopping threads, moment..."));
	thread_stop = 1;
	semaphor_manager::getInstance().stop_all_threads();
	boost::this_thread::sleep_for(boost::chrono::milliseconds(12));
	ce->accept();
}

//Вызывается пользователем. Задаёт частоту генератора
void semaphor_gui::set_generator_frequency(int frequency)
{
	//slot_post_console_msg("[GUI]Set generator frequency " + QString::number(frequency));
	//qDebug() << "[GUI]Set frequency" << QString::number(frequency);
	semaphor_manager::getInstance().set_gen_freq(frequency);
}

//Вызывается пользователем. Задаёт режим работы генератора(вкл или выкл)
void semaphor_gui::switch_generator_mode()
{
	gen_mode = !gen_mode;
	if (gen_mode)
	{
		semaphor_manager::getInstance().run_queue_generator(true);
		slot_post_console_msg(QObject::tr("[GENERATOR]Generator on"));
	}
	else
	{
		semaphor_manager::getInstance().stop_generator();
		slot_post_console_msg(QObject::tr("[GENERATOR]Generator off"));
	}
}

//Добавляет графическое представление светофора с привязкой к ИД на карту
void semaphor_gui::add_graphic_semaphor(const uint16_t& sem_id, const std::pair<int16_t, int16_t>& sem_coord)
{
	s_graph[sem_id]->set_coord(std::move(sem_coord.first), std::move(sem_coord.second));
	//s_graph[sem_id]->set_state(1);
	//semaphor_graphic_arr->push_back(new semaphor_graphic(this, sem_id, sem_coord.first, sem_coord.second));
	//semaphor_graphic_arr->at(s_g_cnt) = new semaphor_graphic(this, sem_id, sem_coord.first, sem_coord.second);
}

void semaphor_gui::set_semaphor_speed(int frequency)
{
	semaphor_manager::getInstance().set_semaphor_speed(static_cast<uint16_t>(frequency));
}

void semaphor_gui::set_refresh_interval_ms(const uint16_t& ms)
{
	refresh_interval = std::move(ms);
}

//Инициализация интерфейса слота панели управления светофором
semaphor_slot::semaphor_slot(QWidget* qwgt) : QGroupBox(qwgt)
{
	myId = 0;

	slot_module = new QVBoxLayout;

	s_color_status = palette();
	s_color_status.setColor(backgroundRole(), QColor(Qt::red).light(80));

	id_box_layout = new QVBoxLayout;
	queue_box_layout = new QVBoxLayout;
	color_box_layout = new QVBoxLayout;
	multi_queue_box_layout = new QVBoxLayout;

	id = new QLabel("NULL");
	queue = new QLabel("NULL");
	color = new QLabel;

	id->setAlignment(Qt::AlignCenter);
	queue->setAlignment(Qt::AlignCenter);
	color->setAlignment(Qt::AlignCenter);

	color->setAutoFillBackground(1);

	lan_connect = new QCheckBox("LAN");

	//Пока что отключаем, до введения функционала
	lan_connect->setCheckable(0);
	lan_connect->setChecked(0);

	multi_queue = new QTextEdit("5");
	multi_queue->setAlignment(Qt::AlignCenter);

	id_box = new QGroupBox(QObject::tr("ID"));
	queue_box = new QGroupBox(QObject::tr("Cnt"));
	color_box = new QGroupBox;
	multi_queue_box = new QGroupBox;

	btn_increment = new QPushButton("+");
	btn_decrement = new QPushButton("-");
	btn_set_queue = new QPushButton(QObject::tr("Set"));

	btn_increment->setAutoFillBackground(1);
	btn_decrement->setAutoFillBackground(1);

	btn_increment->setFixedHeight(28);
	btn_decrement->setFixedHeight(28);

	id_box_layout->addWidget(id);
	queue_box_layout->addWidget(queue);
	color_box_layout->addWidget(color);
	multi_queue_box_layout->addWidget(multi_queue);
	multi_queue_box_layout->addWidget(btn_set_queue);

	id_box->setLayout(id_box_layout);
	queue_box->setLayout(queue_box_layout);
	color_box->setLayout(color_box_layout);
	multi_queue_box->setLayout(multi_queue_box_layout);

	id_box->setFixedHeight(42);
	queue_box->setFixedHeight(42);
	color_box->setFixedHeight(36);

	slot_module->addWidget(lan_connect);
	slot_module->addWidget(id_box);
	slot_module->addWidget(queue_box);
	slot_module->addWidget(color_box);
	slot_module->addWidget(multi_queue_box);
	slot_module->addWidget(btn_increment);
	slot_module->addWidget(btn_decrement);

	color->setPalette(s_color_status);

	this->setLayout(slot_module);
	this->setFixedWidth(68);
	//this->setFixedHeight(360);

	//Подключаем кнопки к слотам
	if (!QObject::connect(btn_increment, SIGNAL(clicked()), this, SLOT(slot_increment_queue())))
		QErrorMessage::qtHandler();
	if (!QObject::connect(btn_decrement, SIGNAL(clicked()), this, SLOT(slot_decrement_queue())))
		QErrorMessage::qtHandler();
	if (!QObject::connect(btn_set_queue, SIGNAL(clicked()), this, SLOT(slot_set_queue_cnt())))
		QErrorMessage::qtHandler();
}

//Для дебага
void semaphor_gui::slot_receive_semaphor_info(std::pair<uint16_t, uint16_t> info)
{
	qDebug() << "Receive signal {" << QString::number(info.first) << QString::number(info.second);
}

//Записывает ИД светофора в панель
void semaphor_slot::slot_set_id(uint16_t sem_id)
{
	myId = sem_id;
	id->setText(QString::number(sem_id));
}

//Вызывается потоком, записывает очередь машин
void semaphor_slot::slot_set_queue(uint16_t queue_cnt)
{
	//THREAD_LOCK
	//qDebug() << "[GUI PANEL ID" << QString::number(myId) << "]Queue size now" << QString::number(queue_cnt);
	QMetaObject::invokeMethod(queue, [=]() {
		queue->setText(QString::number(queue_cnt));
		QString check = queue->text();
		if (check.toInt() != queue_cnt)
		{
			semaphor_gui::getInstance().slot_post_console_msg(QObject::tr("[GUI PANEL ID") + QString::number(myId) + QObject::tr("]Error"));
			//qDebug() << "[GUI PANEL ID" << QString::number(myId) << "]Error";
		}
		});
	//queue->setText(QString::number(queue_cnt));
	//NOTIFY_ALL_THREAD
}

//Записывает состояние светофора(красный или зелёный), вызывается потоком
void semaphor_slot::slot_set_signal(bool ssign)
{
	//THREAD_LOCK
	ssign ? s_color_status.setColor(backgroundRole(), QColor(Qt::green).light(80)) : s_color_status.setColor(backgroundRole(), QColor(Qt::red).light(80));
	//s_color_status.setColor(backgroundRole(), QColor(Qt::green).light(80));
	QMetaObject::invokeMethod(color, [=]() {
		QPalette q_pal = color->palette();
		ssign ? q_pal.setColor(backgroundRole(), QColor(Qt::green).light(80)) : q_pal.setColor(backgroundRole(), QColor(Qt::red).light(80));
		color->setPalette(q_pal);
		});
	//color->setPalette(s_color_status);
	//NOTIFY_ALL_THREAD
}

//Получение ИД панели
uint16_t semaphor_slot::read_my_id()
{
	return myId;
}

//Добавить/убавить очередь на 1 машину
void semaphor_slot::slot_increment_queue()
{
	semaphor_manager::getInstance().increment_semaphor_queue(myId);
}

void semaphor_slot::slot_decrement_queue()
{
	semaphor_manager::getInstance().decrement_semaphor_queue(myId);
}

void semaphor_slot::slot_set_queue_cnt()
{
	QString toInt = multi_queue->toPlainText();
	semaphor_manager::getInstance().addSemaphorQueue(myId, static_cast<uint16_t>(toInt.toInt()));
}

//Конструкторы на все случаи жизни
semaphor_graphic::semaphor_graphic(QWidget* qwgt) : QWidget(qwgt)
{
	s_coord = new QPoint;
	s_coord->setX(0);
	s_coord->setY(0);
	myState = 0;
	paint_g = 0;
	myId = 0;
}

semaphor_graphic::semaphor_graphic(QWidget* qwgt, int16_t xC, int16_t yC) : QWidget(qwgt)
{
	s_coord = new QPoint;
	s_coord->setX(xC);
	s_coord->setY(yC);
	myState = 0;
	paint_g = 0;
	myId = 0;
	//semaphor_gui::getInstance().slot_post_console_msg("[GUI]Set semaphor coord X=" + QString::number(s_coord->x()) + " Y=" + QString::number(s_coord->y()));
}

semaphor_graphic::semaphor_graphic(QWidget* qwgt, int16_t xC, int16_t yC, uint16_t sem_id) : QWidget(qwgt), myId(sem_id)
{
	s_coord = new QPoint;
	s_coord->setX(xC);
	s_coord->setY(yC);
	myState = 0;
	paint_g = 0;
	//semaphor_gui::getInstance().slot_post_console_msg("[GUI]Set semaphor coord X=" + QString::number(s_coord->x()) + " Y=" + QString::number(s_coord->y()));
}

//Перехват события отрисовщика
void semaphor_graphic::paintEvent(QPaintEvent* event)
{
	this->setGeometry(s_coord->x(), s_coord->y(), 48, 64);

	QPainter painter(this);

	if (!paint_g)
	{
		//Рисуем корпус светофора
		painter.setBrush(QBrush(QColor(63, 63, 63)));
		painter.setPen(QPen(QColor(0, 0, 0)));
		painter.drawRect(rect());
		paint_g = 1;
	}

	//Установка состояния для красного
	myState ? painter.setBrush(QBrush(QColor(63, 0, 0))) : painter.setBrush(QBrush(QColor(255, 0, 0)));
	painter.setPen(QPen(QColor(0, 0, 0)));
	painter.drawEllipse(rect().width() / 4, rect().height() / 8, 24, 24);

	//Установка состосния для зелёного
	myState ? painter.setBrush(QBrush(QColor(0, 255, 0))) : painter.setBrush(QBrush(QColor(0, 63, 0)));
	painter.setPen(QPen(QColor(0, 0, 0)));
	painter.drawEllipse(rect().width() / 4, rect().height() / 2, 24, 24);

	//repaint();
}

uint16_t semaphor_graphic::read_my_id()
{
	return myId;
}

void semaphor_graphic::set_id(uint16_t sem_id)
{
	myId = sem_id;
}

//Задание координат светофора
void semaphor_graphic::set_coord(int16_t xC, int16_t yC)
{
	s_coord->setX(xC);
	s_coord->setY(yC);

	this->repaint();
}

//Записать состояние светофора. Вызывается потоком
void semaphor_graphic::set_state(bool state)
{
	myState = state;

	this->repaint();
}
