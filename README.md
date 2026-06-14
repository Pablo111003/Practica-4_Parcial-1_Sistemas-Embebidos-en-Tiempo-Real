# Nombres: Pablo Mansilla Hernández (9129)

# Descripción del Codigo:

En este código se implementa un sistema embebido multitask, donde se tiene una tarea la cual lee continuamente un sensor LDR mediante el ADC, aplicando un filtro de mediana
parametrizable para asi eliminar ruido. El resultado ya filtrado, determina cual es el ángulo objetivo, y se comunica a un coordinador central a traves de colas de FreeRTOS.
El "TaskManager" administra el estado del sistema, y suspende y reanuda las tareas dependiendo de la etapa de operación en la que se encuentre.

# Tabla de pines:

GPIO 34 -> LDR (terminal central) -> Resistencia 10k Ohms -> GND

3.3V -> LDR (terminal superior)

GPIO 25 -> Servomotor (señal PWM)

Vin -> Servomotor (alimentación)

GND -> Servomotor (tierra)

GPIO 22 -> Boton Start -> GND

GPIO 21 -> Boton Velocidad -> GND

GPIO 2 -> LED indicador -> Resistencia 220 Ohms -> GND

# Conclusiones:

Haciendo este codigo, comprendi como es que las colas de FreeRTOS, pueden resolver dos problemas al mismo tiempo, comunicando datos entre tareas de forma segura sin necesitar de un
mutex, ademas de notificar automáticamente a la tarea receptora cuando es que hay información disponible. Ademas de esto, entendi la importancia y beneficios que tiene el concentrar
el control en "TaskManager", ya que, al ser el unico punto que llama a "vTaskSuspend" y a "vTaskResume", eso hace que el estado del sistema siempre sea coherente, 
sin que tareas independientes puedan interferir entre sí.


# Preguntas:

1. ¿Cuál es la diferencia entre usar una variable global y una cola para comunicar datos?
   R= Cuando se utiliza una variable global, se corre el riesgo de que dos tareas accedan al mismo tiempo, provocando que ocurra una condición de carrera y que se lea un
   dato a medias. En cambio, si se usa una cola FreeRTOS, estas son seguras por diseño, ya que se serializan los accesos, y la tarea que lea queda bloqueada automáticamente
   hasta que haya un dato disponible, sin tener que gastar CPU mientras espera a eso.

2. ¿Qué tarea queda bloqueada cuando espera datos de una cola?
   R= Cualquier tarea que llame a "xQueueReceive()", y que tenga un timeout mayor a cero, mientras la cola esta vacia entra en estado de BLOCKED. El scheduler la saca de
   las tareas activas y no le asigna CPU hasta que llegue un dato o expire el timeout.

3. ¿Por qué TaskManager debe concentrar las decisiones del sistema?
   R= Porque esto nos garantiza que el estado del sistema siempre sea coherente. Al tener un solo punto de control se simplifica la depuración, y tambien es más
   sencillo añadir nuevos estados en el futuro.

4. ¿Qué diferencia existe entre suspender una tarea y bloquearla esperando una cola?
   R= Cuando se suspende con "vTaskSuspend()", esa tarea queda fuera del schedule, y solo puede volver cuando otra tarea llame a "vTaskResume()". En cambio, cuando bloquea
   en una cola, la tarea se desbloquea sola cuando llega un dato o expira el timeout.
