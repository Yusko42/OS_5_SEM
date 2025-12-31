## РЕЗУЛЬТАТЫ

**МЬЮТЕКС**
  bash ```
    Mutex size: 1000, 10 s
    iterations_asc:  141429, pairs_asc:  60526422, swap_asc: 164634
    iterations_desc: 141458, pairs_desc: 60492798, swap_desc: 164088
    iterations_eql:  141619, pairs_eql:  20307461, swap_not_equal: 331125
  ```

  bash ```
    Mutex size: 10000, 10 s
    iterations_asc:  21862, pairs_asc:  93739748, swap_asc: 24454
    iterations_desc: 21862, pairs_desc: 93933657, swap_desc: 24427
    iterations_eql:  21861, pairs_eql:  30923269, swap_not_equal: 48846
  ```

  bash ```
    Mutex size: 100000, 10 s
    iterations_asc:  1939, pairs_asc:  83007832, swap_asc: 2408
    iterations_desc: 1938, pairs_desc: 82924652, swap_desc: 2360
    iterations_eql:  1939, pairs_eql:  27922723, swap_not_equal: 4969
  ```
  bash ```
    Mutex size: 1000000, 10 s
    iterations_asc:  202, pairs_asc:  86609475, swap_asc: 232
    iterations_desc: 202, pairs_desc: 86571879, swap_desc: 208
    iterations_eql:  202, pairs_eql:  28818441, swap_not_equal: 460
  ```
**СПИНЛОК**

  bash ```
    SPINLOCK: size: 1000, 10 s
    iterations_asc:  181381, pairs_asc:  77472808, swap_asc: 256488
    iterations_desc: 181853, pairs_desc: 77006433, swap_desc: 272385
    iterations_eql:  181735, pairs_eql:  26970780, swap_not_equal: 506691
  ```

  bash ```
    SPINLOCK: size: 10000, 10 s
    iterations_asc:  16856, pairs_asc:  71984198, swap_asc: 27892
    iterations_desc: 16856, pairs_desc: 72073866, swap_desc: 26669
    iterations_eql:  16888, pairs_eql:  24531131, swap_not_equal: 53124
  ```

  bash ```
    SPINLOCK: size: 100000, 10 s
    iterations_asc:  1689, pairs_asc:  72490136, swap_asc: 2791
    iterations_desc: 1692, pairs_desc: 72384297, swap_desc: 2674
    iterations_eql:  1690, pairs_eql:  24166557, swap_not_equal: 5395
  ```
  
  bash ```
    SPINLOCK: size: 1000000, 10 s
    iterations_asc:  185, pairs_asc:  79270520, swap_asc: 242
    iterations_desc: 173, pairs_desc: 74191997, swap_desc: 247
    iterations_eql:  175, pairs_eql:  24964454, swap_not_equal: 471
  ```

**RWLOCK**

  bash ```
    RWLOCK: size: 1000, 10 s
    iterations_asc:  145695, pairs_asc:  62204084, swap_asc: 163037
    iterations_desc: 145755, pairs_desc: 62071931, swap_desc: 165781
    iterations_eql:  145744, pairs_eql:  21304359, swap_not_equal: 326141
  ```

  bash ```
    RWLOCK: size: 10000, 10 s
    iterations_asc:  17223, pairs_asc:  73498493, swap_asc: 14420
    iterations_desc: 16645, pairs_desc: 71842354, swap_desc: 13671
    iterations_eql:  17385, pairs_eql:  24598910, swap_not_equal: 26949
  ```
  
  bash ```
    RWLOCK: size: 100000, 10 s
    iterations_asc:  1738, pairs_asc:  74626681, swap_asc: 1372
    iterations_desc: 1588, pairs_desc: 68318842, swap_desc: 1352
    iterations_eql:  1665, pairs_eql:  23374756, swap_not_equal: 3062
  ```
  
  bash ```
    RWLOCK: size: 1000000, 10 s
    iterations_asc:  160, pairs_asc:  68578620, swap_incr: 151
    iterations_desc: 181, pairs_desc: 77517052, swap_decr: 146
    iterations_eql:  170, pairs_eql:  24329008, swap_comp: 300
  ```

## ОБЪЯСНЕНИЕ РЕЗУЛЬТАТОВ

С увеличением размера списка наблюдается значительное снижение количества итераций для всех типов блокировок. Это объясняется тем, что полный обход большого списка занимает больше времени, как следствие, за одну итерацию приходится проходить, блокировать/разблокировать всё больше элементов. Поэтому за 10 секунд удается выполнить меньше циклов.
swap_not_equal, как можно заметить, приблизительно в 2 раза больше других счётчиков (swap_asc, swap_desc). Причина в условии, при котором выполняется инкрементирование: left != right, что состоит из > и < (равносильно неравенству), которые уже являются условиями в двух других потоках.

Быстрее всех - спинлок: критические секции достаточно коротки, при ожидании будет ожидать в цикле в user-space.
Медленнее - мьютекс: при отсутствии доступа к мьютексу поток засыпает, переключая контекст.
Баланс - rwlock: на малых списках несколько быстрее, но близко к mutex, потому что критические секции коротки, накладные расходы на управление сложной блокировкой rwlock нивелируют преимущества параллельного чтения. Потоки чтения завершают обход быстро и редко пересекаются во времени.
С ростом размера списка накладные расходы на синхронизацию становятся менее значительными, основное время тратится на обход списка и обработку данных. Эффективно rwlock показывает себя при большем количестве читателей над числом писателей, но поскольку их чсло у нас одинаковое, преимущество теряется из-за числа активных потоков записи, из-за который читателям приходится ожидать освобождения блокровки.

## ЧТО ЗАНЯЛО ВРЕМЯ НА ВЫПОЛНЕНИЕ ЭТОЙ РАБОТЫ?

Столкнулся с критическим падением производительности из-за некорректной работы с генератором случайных чисел. Изначальная реализация инициализировала seed внутри функции перестановки: 
  bash```
    seed = time(NULL) ^ pthread_self()
  ```

Поскольку time(NULL) обновляется раз в секунду, все потоки в течение каждой секунды получали практически одинаковые значения seed, что приводило к генерации одинаковых индексов и постоянной конкуренции за одни и те же узлы списка.

## Решение и результат

Исправление -- перенос инициализации seed на уровень потока с сохранением состояния между вызовами. После этого потоки стали работать с уникальными последовательностями случайных чисел, что устранило конкуренцию и увеличило производительность перестановок в десятки тысяч раз (с единиц до десятков тысяч операций за 10 секунд).