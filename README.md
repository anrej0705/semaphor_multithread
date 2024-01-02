# Semaphors v 1.1a

![Qt](https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![MSVC](https://img.shields.io/badge/Visual_Studio-5C2D91?style=for-the-badge&logo=visual%20studio&logoColor=white)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

**| Qt 5.14.2 
| C++17 
| MSVC 2019 
| Boost 1.84.00
| GPL v2 license |**

Программа моделирует движение на перекрестке по упрощённой модели. Светофоры работают каждый в своей зоне - принадлежность светофоров к зоне можно увидеть в таблице ниже(столбцы - зоны, строки - светофоры)

Таблица 1. Распределение светофоров по зонам
| |1|2|3|4|5|6|
|-|-|-|-|-|-|-|
|1|X||||||
|2|X||||||
|3||X|||||
|4||X|||||
|5||||X|||
|6|||X||||
|7||||X|||
|8|||||X||
|9|||||X||
|10||||||X|
|11|||X||||
|12||||||X|

Для улучшенного движения предусмотрено несколько разрешенных к проезду зон. Это означает что на перекрестке сразу несколько светофоров могут разрешить проезд машин. При этом заны распределены так, чтобы не
создавать аварийной ситуации

Таблица 2. Одновременно разрешенные к проезду зоны
| |1|2|3|4|5|6|
|-|-|-|-|-|-|-|
|1|X|||X||X|
|2||X|X||X||

Если показать на схеме, то это будет выглядеть вот так:


[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/b5/_f114ec7c13e1af6cd88f2c6239308bb5.jpeg)](https://fastpic.org/view/122/2023/1231/_f114ec7c13e1af6cd88f2c6239308bb5.png.html)
[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/88/_f4a5330083e11bac0bad15ae3cac4888.jpeg)](https://fastpic.org/view/122/2023/1231/_f4a5330083e11bac0bad15ae3cac4888.png.html)

Любой из светофоров может запросить информацию о другом светофоре через менеджера используя метод ```semaphor::request_neighbour_status()```
Сначала светофор проверяет состояние соседей. Если любой из соседей, либо все сразу в текущий момент пропускают машины - светофор не разрешит проезд, чтобы не допустить
создания аварийной ситуации. Список соседей можно загрузить в светофор через метод ```semaphor::add_neighbour(uint16_t n_id)``` во внутреннюю таблицу светофора. 
Например для светофора №3 из рисунка выше соседями будут №1 и №2, а для №2 - №3 и №4

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/60/9200fba87cb805837a973e5228c5cd60.jpeg)](https://fastpic.org/view/122/2023/1231/9200fba87cb805837a973e5228c5cd60.png.html)

Все запросы светофоров между собой проходят через менджера, и ответы в том числе

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/3c/a35787c307c529dcd4fc41321534bf3c.jpeg)](https://fastpic.org/view/122/2023/1231/a35787c307c529dcd4fc41321534bf3c.png.html)
[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/e4/a1f0efc193dab281187d599763611be4.jpeg)](https://fastpic.org/view/122/2023/1231/a1f0efc193dab281187d599763611be4.png.html)

Если все соседи имеют красный сигнал, значит дорога теоретически свободна что позволяет перейти к следующему шагу - запросу разрешения у менеджера. Менеджер получает запрос и сверяется
со своей таблицей приоритетов которая строится исходя из показаний загруженности светофоров - самый загруженный получает наивысший приоритет. Частота расчета таблицы загруженности может
меняться как и частота запросов к менеджеру, в зависимости от ситуации на дороге

### Интерфейс

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/69/_ddf8eb728de1d73ec69db57b0b5c9069.jpeg)](https://fastpic.org/view/122/2023/1231/_ddf8eb728de1d73ec69db57b0b5c9069.png.html)

Программа оснащена простым простым интерфейсом не включающим в себя дополнительные окна или меню. Слева находится карта на которой расположены светофоры со стрелками - поток
который контролирует светофор.


[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/99/9a9b051327016a382ece03e59db14099.jpeg)](https://fastpic.org/view/122/2023/1231/9a9b051327016a382ece03e59db14099.png.html)

Справа 3 панели - верхняя панель генератора - прездназначена для управления автоматической генерации очереди перед светфором. Следующая под ним - панель
со слотами где каждый слот отображает состояние привязанного к нему светофора. На выбор можно прибавить/уменьшить очередь на любом из них в любом порядке. Нижняя панель включает в себя
таблицу и консоль для пользователя в которой отображается информация о нажатых клавишах или произошедших событиях(например настройка в начале работы или инфо о последних нажатых кнопках
в слоте). Таблица отображает информацию о светофорах которая содержится в менеджере и которую менеджер периодически обновляет

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/84/01676f0b0a797df21d0d34c16c9bf084.jpeg)](https://fastpic.org/view/122/2023/1231/01676f0b0a797df21d0d34c16c9bf084.png.html)

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/28/80ed25bbf2134672ba00082b08e33f28.jpeg)](https://fastpic.org/view/122/2023/1231/80ed25bbf2134672ba00082b08e33f28.png.html)

[![FastPic.Ru](https://i122.fastpic.org/thumb/2023/1231/9c/e2eeed1169781110195aeb00627ad69c.jpeg)](https://fastpic.org/view/122/2023/1231/e2eeed1169781110195aeb00627ad69c.png.html)

### Управление

Управлять программой просто из-за малого количества элементов управления. Генератор можно в любой момент времени включить или изменить скорость - эти действия можно производить в 
любом порадке. Генератор каждый цикл добавляет 1 машину к случайному светоформу, в его алгоритме используется генератор случайных чисел по вихрю Мерсена - ```mt19937``` 
для получения более приближенных к истинно случайным значениям. Панель из 12 слотов отображает состояние светофора с ид указанным в шапке слота и содержит 2 кнопки, нажимая которые
в любом порядке можно вручную напрямую менять количество машин в очереди светофора. Выше кнопок содержится окошко которое цветом отображает состояние светофора и выше окошка - количество
условных маших/пешеходов в очереди перед светофором - пропуск очередь начнётся порционно так как алгоритм работы менеджера распределяет потоки так чтобы не было застойных зон, когда
один из светофоров ждёт слишком долго, или наоборот слишком долго пропускает пустую очередь. Проверить работу алгоритма можно как вручную, так и с помощью генератора - переведя его в 
активный режим и задав максимальную частоту генераций

С версии 1.1: стала доступна регулировка скорости работы светофоров и установка произвольного количества машин в очереди - её можно повысить или понизить. Ползунок находится в 
модуле со слотами управления светофорами. Установка произвольной длины очереди доступна из панели слота и, соответственно, очередь может устанавливаться на любой выбранный светофор 
или светофоры по отдельности - можно увидеть из скрина снизу

[![FastPic.Ru](https://i122.fastpic.org/thumb/2024/0101/08/b90a9a919355ed73021ed3110ccfde08.jpeg)](https://fastpic.org/view/122/2024/0101/b90a9a919355ed73021ed3110ccfde08.png.html)

### Дополнительный настройки

С версии 1.1a стала доступна возможность изменять некоторые настройки программы, не отображающиеся в окне. Для этого следует открыть файл ```param.xml```(если его нет то запустите
программу и он сгенерируется). Структура файла выглядит следующим образом:

```
<!DOCTYPE __Shani_basic>
<__Shani_basic>
 <Settings attr="uint16_t">
  <param>semaphor_speed_cycle</param>
  <value>42</value>
 </Settings>
 <Settings attr="uint16_t">
  <param>generator_speed_cycle</param>
  <value>4</value>
 </Settings>
 <Settings attr="uint16_t">
  <param>manager_speed_cycle</param>
  <value>10</value>
 </Settings>
 <Settings attr="uint16_t">
  <param>gui_refresh_cycle</param>
  <value>50</value>
 </Settings>
 <Settings attr="uint16_t">
  <param>gui_refresh_interval</param>
  <value>65535</value>
 </Settings>
</__Shani_basic>

```

``` __Shani_basic ``` - Сигнатура по которой программа определяет что открыт файл содержащий её настройки

``` uint16_t ``` - Указывает на тип данных - unsigned 16 bit int с диапазоном значений от 0 до 65535

``` semaphor_speed_cycle ``` - Установка длительности цикла для светофоров. Стандартное значение составляет **42 мс**. Не рекомендуется устаналивать более низкие значения - это может
привести к багам в работе или "зависанию" потока

``` generator_speed_cycle ``` - Установка длительности цикла для генератора. Стандартное значение **4 мс**. Не рекомендуется выставлять 0.

``` manager_speed_cycle ``` - Установка цикла для менеджера. Стандартное значение **10 мс**. Рекомендуемый минимум 2 мс.

``` gui_refresh_cycle ``` - Установка частоты обновления графического интерфейса. Стандартное значение **50 мс** что составляет 20 гц. Не рекомендуется к изменению - может привести
к багам либо тормозам интерфейса

``` gui_refresh_interval ``` - Не используемый в данный момент параметр
