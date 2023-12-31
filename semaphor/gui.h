#pragma once
#include <QtWidgets>
#include "boost/container/vector.hpp"
#include "boost/thread/thread.hpp"

class semaphor_slot;
class semaphor_gui;
class semaphor_graphic;

class singlet_destroyer2
{
	private:
		semaphor_gui* m_inst;
	public:
		~singlet_destroyer2();
		void initialize(semaphor_gui*);
};
class semaphor_gui : public QWidget
{
	Q_OBJECT
	private:
		static semaphor_gui* s_slot_ptr;
		static singlet_destroyer2 dstr;

		QLabel* background_img;
		QPixmap* background_pxmap;
		QHBoxLayout* hLay; //���������� ������ � ��������� � ������ ����������

		bool thread_stop;
		bool gen_mode;

		uint16_t slot_cnt;
		uint16_t s_g_cnt;
		
		//������ ��������� ����������
		QWidget* control_panel;	//������ ����������
		QVBoxLayout* control_lay; //���������� ������� ������ ����������

		//���������� ������ ������ ����������
		QGroupBox* generator_module;	//������ ����������
		QHBoxLayout* state_machine_lay;	//���������� ��������� ����������
		QHBoxLayout* slider_lay;		//�������
		QVBoxLayout* generator_lay;		//����������
		QPushButton* mode_switcher;	//������������� ���������
		QSlider* gen_rate_slider;	//�������� ����������� �������� ���������
		QStateMachine* generator_mode;	//������ ���������
		QState* mode_en;	//��������� ������
		QState* mode_dis;	//��������� �������
		QLabel* lbl_en;		//������� ���
		QLabel* lbl_dis;	//������� ����
		QLabel* slider_indicator;	//��������� �� ��������
		QLabel* lbl_frequency;	//"������� ���������"

		//������ �� �������-�����������
		QGroupBox* slots_module;
		QHBoxLayout* slots_module_lay;

		boost::container::vector<std::pair<uint16_t, bool>> semaphor_state;
		boost::container::vector<std::pair<uint16_t, uint16_t>> semaphor_gueue_cnt;

		boost::container::vector<semaphor_slot*>* semaphor_slots;
		semaphor_graphic* s_graph[12];
		boost::thread* gui_thread;

		//������� � �������
		QHBoxLayout* console_lay; //���������� ������� MV � �������
		QGroupBox* console_box;
		QGroupBox* table_box;
		QVBoxLayout* console_box_lay;
		QVBoxLayout* table_box_lay;
		QTextEdit* console;
		QStandardItemModel* table;
		QTreeView* table_view;
	protected:
		semaphor_gui(QWidget* qwgt = 0);
		semaphor_gui(const semaphor_gui&);
		semaphor_gui& operator=(semaphor_gui&);
		~semaphor_gui();
		friend class singlet_destroyer2;
	public:
		static semaphor_gui& getInstance();
		void add_slot(uint16_t sem_id);
		void add_graphic_semaphor(uint16_t sem_id, std::pair<int16_t, int16_t>);
		void write_queue_cnt(uint16_t sem_id, uint16_t queue_cnt);
		void write_table_content(uint8_t section_num, uint8_t row_num, uint16_t arg);
		void set_signal(uint16_t sem_id, bool signal);
		void run_gui();
		void gui();
		void closeEvent(QCloseEvent* ce);
	public slots:
		void set_generator_frequency(int frequency);
		void slot_receive_semaphor_info(std::pair<uint16_t, uint16_t>);
		void slot_post_console_msg(QString msg);
		void switch_generator_mode();
		void showGUI();

};

class semaphor_slot : public QGroupBox
{
	Q_OBJECT
	private:
		QPalette s_color_status;

		QVBoxLayout* slot_module;	//����������� ���������
		
		QVBoxLayout* id_box_layout;
		QVBoxLayout* queue_box_layout;
		QVBoxLayout* color_box_layout;

		QLabel* id;		//�� ���������
		QLabel* queue;	//������� �������
		QLabel* color;	//����(������)

		QGroupBox* id_box;
		QGroupBox* queue_box;
		QGroupBox* color_box;

		QPushButton* btn_increment;	//�������� ������� +1
		QPushButton* btn_decrement;	//������� ������� -1
	public:
		uint16_t myId;

		semaphor_slot(QWidget* qwgt = 0);
		void slot_set_id(uint16_t);
		void slot_set_queue(uint16_t);
		void slot_set_signal(bool ssing);
		uint16_t read_my_id();
	public slots:
		void slot_increment_queue();
		void slot_decrement_queue();
};

class semaphor_graphic : public QWidget
{
	Q_OBJECT
	private:
		QPoint* s_coord;
		uint16_t myId;
		bool myState;
		bool paint_g;
	protected:
		void paintEvent(QPaintEvent* event);
	public:
		semaphor_graphic(QWidget* qwgt);
		semaphor_graphic(QWidget* qwgt, int16_t xC, int16_t yC);
		semaphor_graphic(QWidget* qwgt, int16_t xC, int16_t yC, uint16_t sem_id);
		uint16_t read_my_id();
		void set_id(uint16_t);
		void set_coord(int16_t xC, int16_t yC);
		void set_state(bool state);
};
