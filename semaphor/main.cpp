#include "gui.h"
#include "manager.h"
#include "semaphor.h"
#include <QtWidgets/QApplication>
#include <QtWidgets>
#include <thread>

/* Таблица 1. Зависимости светофоров пешеходы->проезжие(какие светофоры кого запрашивают)
* _____________________
* |   | 1 | 2 | 3 | 4 |
* | 5 |   |   | X |   |
* | 6 | X |   |   |   |
* | 7 |   |   | X |   |
* | 8 |   | X |   |   |
* | 9 |   | X |   |   |
* |10 |   |   |   | X |
* |11 | X |   |   |   |
* |12 |   |   |   | X |
*/

/* Таблица 2. Зависимости светофоров проезжие-проезжие(какие светофоры кого запрашивают)
* _____________________
* |   | 1 | 2 | 3 | 4 |
* | 1 |   |   | X | X |
* | 2 |   |   | X | X |
* | 3 | X | X |   |   |
* | 4 | X | X |   |   |
*/

/* Таблица 3. Зоны проезда(параллельные светофоры)
* _____________________________
* |   | 1 | 2 | 3 | 4 | 5 | 6 |
* | 1 | X |   |   |   |   |   |
* | 2 | X |   |   |   |   |   |
* | 3 |   | X |   |   |   |   |
* | 4 |   | X |   |   |   |   |
* | 5 |   |   |   | X |   |   |
* | 6 |   |   | X |   |   |   |
* | 7 |   |   |   | X |   |   |
* | 8 |   |   |   |   | X |   |
* | 9 |   |   |   |   | X |   |
* |10 |   |   |   |   |   | X |
* |11 |   |   | X |   |   |   |
* |12 |   |   |   |   |   | X |
*/

/* Таблица 4. Одновременно разрешенные зоны
* _____________________________
* |   | 1 | 2 | 3 | 4 | 5 | 6 |
* | 1 | X |   |   | X |   | X |
* | 2 |   | X | X |   | X |   |
*/

//Зависимости светофоров проезжей части, см табл. 1,2
typedef boost::container::vector<uint16_t> road_deps;
typedef boost::container::vector<uint8_t> zone_deps;
typedef boost::container::vector<std::pair<uint8_t, uint8_t>> poll_deps;
typedef boost::container::vector<std::pair<int16_t, int16_t>> s_graphic_coords;
road_deps s_r_deps_arr[12] = { 
    { 0x0002,0x0003 },    //1
    { 0x0002,0x0003 },    //2
    { 0x0000,0x0001 },    //3
    { 0x0000,0x0001 },    //4
    { 0x0002 },           //5
    { 0x0000 },           //6
    { 0x0002 },           //7
    { 0x0001 },           //8
    { 0x0001 },           //9
    { 0x0003 },           //10
    { 0x0000 },           //11
    { 0x0003 }            //12
};

//Зоны светофоров, см табл. 3
road_deps s_zone_arr[6] = {
    { 0x0000, 0x0001 },
    { 0x0002, 0x0003 },
    { 0x0005, 0x000A },
    { 0x0004, 0x0006 },
    { 0x0007, 0x0008 },
    { 0x0009, 0x000B }
};

//Одновременно разрешенные зоны, см. табл. 4
zone_deps s_z_deps[2] = {
    { 0x00, 0x03, 0x05 },
    { 0x01, 0x02, 0x04 }
};

//Зависимость частоты опроса от количества очереди перед светофором, см Рис.1
//first - количество машин в очереди
//second - длина цикла
poll_deps p_deps = {
    { { 1,32 }, { 2,24 }, { 3, 18 }, { 4, 13 }, { 5,10 }, { 12,8 }, { 19, 5 }, { 25, 4 }, { 54, 3 }, { 77,2 }, { 99,1 }}
};

//Таблица координат светофоров
s_graphic_coords s_coords = {{
    { 291, 260 }, { 449, 472 }, { 486, 288 }, { 255, 437 }, 
    { 612, 198 }, { 528, 126 }, { 607, 528 }, { 527, 597 }, 
    { 216, 598 }, { 132, 529 }, { 215, 126 }, { 133, 194 }
}};

//Имена для светофоров
typedef std::string semaphor_names;
semaphor_names s_names[12] = {
    "   ROAD   ","   ROAD   ","   ROAD   ","   ROAD   ",
    "PEDESTRIAN","PEDESTRIAN","PEDESTRIAN","PEDESTRIAN","PEDESTRIAN","PEDESTRIAN","PEDESTRIAN","PEDESTRIAN"
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "Adding semaphors";

    QTranslator q_translator;
    q_translator.load(QString("semaphor_ru_RU.qm"));
    qApp->installTranslator(&q_translator);

    semaphor_gui::getInstance().show(); //СИНГЛТОН-ИНТЕРФЕЙС ХЕХЕХЕХХЕХЕХХЕХЕ
    for (uint8_t a = 0; a < 12; ++a)
    {
        semaphor_gui::getInstance().add_slot(a);
        semaphor_gui::getInstance().add_graphic_semaphor(a, s_coords.at(a));
    }

    for (uint8_t a = 0; a < 12; ++a)
        semaphor_manager::getInstance().create_semaphor();
    for (uint8_t a = 0; a < 6; ++a)
        semaphor_manager::getInstance().add_semaphor_to_map(s_zone_arr[a]);
    for (uint8_t a = 0; a < p_deps.size(); ++a)
        semaphor_manager::getInstance().set_polling_graph(p_deps.at(a));
    for (uint8_t a = 0; a < 12; ++a)
    {
        //semaphor_manager::getInstance().create_semaphor();
        semaphor_manager::getInstance().set_neighbour(a, s_r_deps_arr[a]);
        semaphor_manager::getInstance().set_semaphor_name(a, s_names[a]);
        semaphor_manager::getInstance().copy_polling_graph(a);
        if (a < 2)
            semaphor_manager::getInstance().add_parallel_zones(s_z_deps[a]);
    }

    //semaphor_manager::getInstance().run_queue_generator(true);
    semaphor_manager::getInstance().run_manager();
    semaphor_gui::getInstance().run_gui();

    return a.exec();
}
