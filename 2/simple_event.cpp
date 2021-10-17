#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <list>
#include <iostream>

using namespace std;

// Очень простой пример построения имитационной модели с календарём событий 
// Модель "самодостаточная" - не используются библиотеки для организации имитационного моделирования
// Возможности С++ используются недостаточно. Что можно улучшить в этой части?


class Event {                                                       // событие в календаре
public:
    float time;                                                     // время свершения события
    int type;                                                       // тип события
    // если тип равен EV_REQ, то в поле attr находится номер клиента, отправившего запрос
    // если тип равен EV_FIN, то в поле attr лежит номер сервера, который выполнял вычисления
    int attr;                                                       // дополнительные сведения о событии в зависимости от типа
    Event(float t, int tt, int a, int ats = -1) : time(t), type(tt), attr(a) { } 
};

                                                                    // типы событий
#define EV_INIT 1
#define EV_REQ 2
#define EV_FIN 3

                                                                    // состояния
#define RUN 1
#define IDLE 0

#define LIMIT 1000                                                  // время окончания моделирования

class Request  {                                                    // задание в очереди
public:
    float time;                                                     // время выполнения задания без прерываний 
    int source_num;                                                 // номер источника заданий (1 или 2)
    Request(float t, int s) : time(t), source_num(s) { } 
};  

class Calendar: public list<Event*> {                               // календарь событий
public:
    void put (Event* ev);                                           // вставить событие в список с упорядочением по полю time
    Event* get ();                                                  // извлечь первое событие из календаря (с наименьшим модельным временем)
};

void Calendar::put(Event *ev) {
    list<Event*>::iterator i;
    Event** e = new (Event*);
    *e = ev;
    if(empty()) {
        push_back(*e);
        return;
    }
    for(i = begin(); (i != end()) && ((*i)->time <= ev->time); ++i) { }
    insert(i, *e);
} 

Event* Calendar::get()
{  
    if(empty()) {
        return NULL;
    } 
    Event *e = front(); 
    pop_front();
    return e;
}


typedef list<Request*> Queue;                                       // очередь заданий к процессору 


class Balancer {
public:
    Balancer(Queue* queues) : qs(queues), server_to_give(0) { }
    Queue* qs;
    int server_to_give = 0;
    void put(Request* req) {
        qs[server_to_give].push_back(req);
        cout << "[BALNCR]: задание от клиента " << req->source_num << " направлено серверу с номером " << server_to_give << endl;
        server_to_give = (server_to_give + 1) % 2;
    }
};


float get_req_time(int source_num);                                 // длительность задания
float get_pause_time(int source_num);                               // длительность паузы между заданиями


int main(int argc, char **argv ) {
    Calendar calendar;
    Queue queues[2];
    float run_begin[2];
    int cpu_state[2] = { IDLE, IDLE };
    Balancer balancer(queues);
    float curr_time = 0;
    Event *curr_ev;
    float dt;
    srand(2019);
                                                                    // начальное событие и инициализация календаря
    curr_ev = new Event(curr_time, EV_INIT, 0);
    calendar.put(curr_ev);

    while((curr_ev = calendar.get()) != NULL) {                     // цикл по событиям
        cout << "[MODEL] : time " << curr_ev->time << " type " << curr_ev->type << endl;
        curr_time = curr_ev->time;                                  // продвигаем время
        // обработка события
        if(curr_time >= LIMIT) {
            cout << "[MODEL] : завершение работы программы - время вышло." << endl;
            break;                                                  // типичное дополнительное условие останова моделирования
        }
        switch(curr_ev->type) {
            case EV_INIT:  // запускаем генераторы запросов
                cout << "[EV_INIT]: Начало моделирования" << endl;
                calendar.put(new Event(curr_time, EV_REQ, 1));  
                calendar.put(new Event(curr_time, EV_REQ, 2));  
            break;
            case EV_REQ:
                // планируем событие окончания обработки, если один из серверов свободен, иначе просим балансировщик положить запрос в нужную очередь
                dt = get_req_time(curr_ev->attr); 
	            cout << "[EV_REQ]: dt " << dt << " num " << curr_ev->attr << endl;
                int assigned_to_server;
                if(cpu_state[0] == IDLE) {
                    assigned_to_server = 0;
	                cpu_state[assigned_to_server] = RUN; 
	                calendar.put(new Event(curr_time + dt, EV_FIN, 0));
	                run_begin[0] = curr_time;
	            } else if(cpu_state[1] == IDLE) {
                    assigned_to_server = 1;
                    cpu_state[assigned_to_server] = RUN;
                    calendar.put(new Event(curr_time + dt, EV_FIN, 1));
                    run_begin[assigned_to_server] = curr_time;
                } else {
 	                balancer.put(new Request(dt, curr_ev->attr));  
                }
                // планируем событие генерации следующего задания
                calendar.put(new Event(curr_time+get_pause_time(curr_ev->attr), EV_REQ, curr_ev->attr)); 
	        break;
            case EV_FIN:
                // объявляем сервер свободным и размещаем задание из очереди, если таковое есть
                int finished_server = curr_ev->attr;
                cpu_state[finished_server] = IDLE;
                // выводим запись о рабочем интервале
                cout << "[EV_FIN]: Работа сервера " << finished_server + 1 <<  " с " << run_begin[finished_server] << " по " << curr_time << " длит. " << (curr_time-run_begin[finished_server]) << endl; 
                if (!queues[finished_server].empty()) {
	                Request *rq = queues[finished_server].front(); 
	                queues[finished_server].pop_front(); 
	                calendar.put(new Event(curr_time + rq->time, EV_FIN, finished_server)); 
	                delete rq; 
	                run_begin[finished_server] = curr_time;
                }
            break;
        } // switch
        delete curr_ev;
    } // while
} // main

int rc = 0; int pc = 0;
float get_req_time(int source_num) {
// Для демонстрационных целей - выдаётся случайное значение
// при детализации модели функцию можно доработать
    double r = ((double)rand())/RAND_MAX;
    cout << "req " << rc << endl; rc++;
    if(source_num == 1) {
        return r*10;
    } else  {
        return r*20; 
    }
}

float get_pause_time(int source_num) { // длительность паузы между заданиями  
// см. комментарий выше
    double p = ((double)rand())/RAND_MAX;
    cout << "pause " << pc << endl; pc++;
    if(source_num == 1) { 
        return p*20;
    } else {
        return p*10; 
    }
}

