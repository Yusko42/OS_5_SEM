## РЕЗУЛЬТАТЫ

**МЬЮТЕКС**
```bash
    MUTEX: size: 1000, 10 s
    iterations_asc:  120849, pairs_asc:  51462531, swap_asc: 131748
    iterations_desc: 120792, pairs_desc: 51541113, swap_desc: 132443
    iterations_eql:  120856, pairs_eql:  17701436, swap_not_equal: 262128
```

```bash
    MUTEX: size: 10000, 10 s
    iterations_asc:  16471, pairs_asc:  70432359, swap_asc: 18704
    iterations_desc: 16470, pairs_desc: 70571321, swap_desc: 18801
    iterations_eql:  16471, pairs_eql:  23685663, swap_not_equal: 38337
```

```bash
    MUTEX: size: 100000, 10 s
    iterations_asc:  1575, pairs_asc:  67374833, swap_asc: 1915
    iterations_desc: 1576, pairs_desc: 67581905, swap_desc: 1793
    iterations_eql:  1574, pairs_eql:  22570228, swap_not_equal: 3567
```
```bash
    MUTEX: size: 1000000, 10 s
    iterations_asc:  160, pairs_asc:  68506462, swap_asc: 176
    iterations_desc: 161, pairs_desc: 69097645, swap_desc: 168
    iterations_eql:  159, pairs_eql:  22682258, swap_not_equal: 330
```

**СПИНЛОК**

```bash
    SPINLOCK: size: 1000, 10 s
    iterations_asc:  177669, pairs_asc:  76316763, swap_asc: 222481
    iterations_desc: 177720, pairs_desc: 76229281, swap_desc: 217571
    iterations_eql:  177690, pairs_eql:  24969464, swap_not_equal: 440018
```

```bash
    SPINLOCK: size: 10000, 10 s
    iterations_asc:  17963, pairs_asc:  76806952, swap_asc: 21691
    iterations_desc: 17963, pairs_desc: 77289691, swap_desc: 22760
    iterations_eql:  17969, pairs_eql:  25523916, swap_not_equal: 43488
```

```bash
    SPINLOCK: size: 100000, 10 s
    iterations_asc:  1880, pairs_asc:  80746830, swap_asc: 2395
    iterations_desc: 1871, pairs_desc: 80279059, swap_desc: 2250
    iterations_eql:  1878, pairs_eql:  26557732, swap_not_equal: 4487
```
  
```bash
    SPINLOCK: size: 1000000, 10 s
    iterations_asc:  157, pairs_asc:  67246965, swap_asc: 205
    iterations_desc: 158, pairs_desc: 67770540, swap_desc: 209
    iterations_eql:  163, pairs_eql:  23267750, swap_not_equal: 439
```

**RWLOCK**

```bash
    RWLOCK: size: 1000, 10 s
    iterations_asc:  94783, pairs_asc:  40418782, swap_asc: 87192
    iterations_desc: 95440, pairs_desc: 40539372, swap_desc: 86480
    iterations_eql:  96033, pairs_eql:  14197081, swap_not_equal: 175297
```

```bash
    RWLOCK: size: 10000, 10 s
    iterations_asc:  12554, pairs_asc:  54053839, swap_asc: 10670
    iterations_desc: 12885, pairs_desc: 55259767, swap_desc: 10369
    iterations_eql:  12704, pairs_eql:  17840063, swap_not_equal: 21146
```
  
```bash
    RWLOCK: size: 100000, 10 s
    iterations_asc:  1161, pairs_asc:  49645308, swap_asc: 1041
    iterations_desc: 1162, pairs_desc: 49813542, swap_desc: 1095
    iterations_eql:  1213, pairs_eql:  17429363, swap_not_equal: 2228
```
  
```bash
    RWLOCK: size: 1000000, 10 s
    iterations_asc:  139, pairs_asc:  59603828, swap_asc: 112
    iterations_desc: 141, pairs_desc: 60428736, swap_desc: 117
    iterations_eql:  154, pairs_eql:  21963775, swap_not_equal: 210
```

## ОБЪЯСНЕНИЕ РЕЗУЛЬТАТОВ

С увеличением размера списка наблюдается значительное снижение количества итераций для всех типов блокировок. Это объясняется тем, что полный обход большого списка занимает больше времени, как следствие, за одну итерацию приходится проходить, блокировать/разблокировать всё больше элементов. Поэтому за 10 секунд удается выполнить меньше циклов.
swap_not_equal, как можно заметить, приблизительно в 2 раза больше других счётчиков (swap_asc, swap_desc). Причина в условии, при котором выполняется инкрементирование: left != right, что состоит из > и < (равносильно неравенству), которые уже являются условиями в двух других потоках.

Быстрее всех - спинлок: критические секции достаточно коротки, без переключений контекста (при ожидании будет ожидать в цикле в user-space).
Баланс - мьютекс: при отсутствии доступа к мьютексу поток засыпает, переключая контекст, что экономит CPU, но системные вызовы создают накладные расходы.
Медленнее всех - rwlock: накладные расходы на управление сложной блокировкой rwlock нивелируют преимущества параллельного чтения. Потоки чтения завершают обход быстро и редко пересекаются во времени. Эффективно rwlock показывает себя при большем количестве читателей над числом писателей, но поскольку их число у нас одинаковое, преимущество теряется из-за числа активных потоков записи, из-за который читателям приходится ожидать освобождения блокировки.

С ростом размера списка накладные расходы на синхронизацию становятся менее значительными, основное время тратится на обход списка и обработку данных.

## ЧТО ЗАНЯЛО ВРЕМЯ НА ВЫПОЛНЕНИЕ ЭТОЙ РАБОТЫ?

Столкнулся с критическим падением производительности из-за некорректной работы с генератором случайных чисел. Изначальная реализация инициализировала seed внутри функции перестановки: 
  bash```
    seed = time(NULL) ^ pthread_self()```

Поскольку time(NULL) обновляется раз в секунду, все потоки в течение каждой секунды получали практически одинаковые значения seed, что приводило к генерации одинаковых индексов и постоянной конкуренции за одни и те же узлы списка.

## Решение и результат

Исправление -- перенос инициализации seed на уровень потока с сохранением состояния между вызовами. После этого потоки стали работать с уникальными последовательностями случайных чисел, что устранило конкуренцию и увеличило производительность перестановок в десятки тысяч раз (с единиц до десятков тысяч операций за 10 секунд).