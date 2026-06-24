/**
 * =============================================================================
 * CONCLUSION DEL EQUIPO
 * Integrantes: Pablo Mansilla Hernández
 *
 * Haciendo este codigo, comprendi como es que las colas de FreeRTOS, pueden resolver dos problemas 
 * al mismo tiempo, comunicando datos entre tareas de forma segura sin necesitar de un mutex, ademas 
 * de notificar automáticamente a la tarea receptora cuando es que hay información disponible. 
 * Ademas de esto, entendi la importancia y beneficios que tiene el concentrar el control en 
 * "TaskManager", ya que, al ser el unico punto que llama a "vTaskSuspend" y a "vTaskResume", eso 
 * hace que el estado del sistema siempre sea coherente, sin que tareas independientes puedan interferir 
 * entre sí.
 * 
 * 
 * ¿Cuál es la diferencia entre usar una variable global y una cola para comunicar datos?
 * R= Cuando se utiliza una variable global, se corre el riesgo de que dos tareas accedan al mismo 
 * tiempo, provocando que ocurra una condición de carrera y que se lea un dato a medias. En 
 * cambio, si se usa una cola FreeRTOS, estas son seguras por diseño, ya que se serializan los 
 * accesos, y la tarea que lea queda bloqueada automáticamente hasta que haya un dato disponible, sin 
 * tener que gastar CPU mientras espera a eso.
 * 
 * ¿Qué tarea queda bloqueada cuando espera datos de una cola?
 * R= Cualquier tarea que llame a "xQueueReceive()", y que tenga un timeout mayor a cero, mientras 
 * la cola esta vacia entra en estado de BLOCKED. El scheduler la saca de las tareas activas y no le 
 * asigna CPU hasta que llegue un dato o expire el timeout.
 * 
 * ¿Por qué TaskManager debe concentrar las decisiones del sistema? 
 * R= Porque esto nos garantiza que el estado del sistema siempre sea coherente. Al tener un solo 
 * punto de control se simplifica la depuración, y tambien es más sencillo añadir nuevos estados en 
 * el futuro.
 * 
 * ¿Qué diferencia existe entre suspender una tarea y bloquearla esperando una cola? 
 * R= Cuando se suspende con "vTaskSuspend()", esa tarea queda fuera del schedule, y solo puede volver 
 * cuando otra tarea llame a "vTaskResume()". En cambio, cuando bloquea en una cola, la tarea se 
 * desbloquea sola cuando llega un dato o expira el timeout.
 * 
 * 
 * =============================================================================
 */

#include "task_manager.hpp"

extern "C" void app_main(void)
{
    App::app_tasks_create();
}
