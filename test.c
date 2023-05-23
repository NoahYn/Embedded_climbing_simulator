#include "INCLUDES.H"

#define TASK_STK_SIZE 512
#define N_TASK 5
#define N_MSG 10
#define MAX_PITCHES 10 // total pitch of one route
#define N_ROUTE 3 // number of routes in map
#define BUFSIZE 100 
#define ROPE_LEN 60 // length of rope
#define BLOODTYPE "A+" 
#define WEIGHT 70
#define IMPACT 12000 // impact force that rope can endure
#define G 9.8 // gravity constant

struct pitch {
	INT8U len; // length of pitch
	char grade[10]; // grade of pitch : it indicate how difficult the pitch is
	char name[20]; // route name
};

OS_STK TaskStk[N_TASK][TASK_STK_SIZE]; // Task Stack Memory allocate

OS_EVENT* sem;
OS_EVENT* msg_C2B; // massege queue structure for climber to belayer
OS_EVENT* msg_B2C; // message queue structure for belayer to climber
void* marr_c2b[N_MSG];
void* marr_b2c[N_MSG];
INT8U route; // number of route
INT8U pos; // current position(pitch) of route
INT8U total; // total time 
INT8 rope_drb = 5; // number of falls rope can withstand
BOOLEAN flash;

struct pitch routes[N_ROUTE][MAX_PITCHES]; // route info from map.txt

void GetRouteinfo(void* pdata); // read route map and load information
void Climber_sign(void* data); // sign form climber
void Belayer_sign(void* data); // sign from belayer
void Rescue_request(void* pdata); // in emergency situation, request a rescue
void Check_rope(void* data); // check whether rope is safe or not. 

//void LogTask(void* data);

int main(void) {
	OSInit(); // uC/OS-II initialize
	srand(time(NULL)); // rand seed initialize

	OSTaskCreate(GetRouteinfo, (void*)0, &TaskStk[0][TASK_STK_SIZE - 1], 10);

	sem = OSSemCreate(1);

	msg_C2B = OSQCreate(marr_c2b, (INT16U)N_MSG);
	msg_B2C = OSQCreate(marr_b2c, (INT16U)N_MSG);
	if (msg_C2B == 0 || msg_B2C == 0) {
		printf("Creating a massage queue is failed!\n");
		return -1;
	}

	OSStart(); // multi-tasking start
	return 0;
}

void GetRouteinfo(void* pdata) {
	FILE* map;
	INT8U buf[BUFSIZE];
	INT8 routeIndex = -1;
	INT8U pitchIndex = 0;

	while (1) {
		if (routes[0][0].len == 0) { // route information is not loaded yet
			map = fopen("source/map.txt", "r");
			if (!map) {
				printf("fail to open file.\n");
				continue;
			}

			while (fgets(buf, BUFSIZE, map) != NULL) {
				if (strstr(buf, "Route name") != NULL) { // head of route information
					if (routeIndex >= 0 && pitchIndex > 0) { // counted previous total number of pitch
						routes[routeIndex][0].len = pitchIndex - 1; // save total number of pitch
					}
					routeIndex++; // next route
					pitchIndex = 1; // reset pitch index
					sscanf(buf, "Route name : %s", routes[routeIndex][0].name); // save the route name
				}
				else if (strstr(buf, "p") != NULL) { // body of route information
					sscanf(buf, "%*hhdp : %hhdm,", &routes[routeIndex][pitchIndex].len); // save length of this pitch
					sscanf(buf, "%*[^,], %[^\n]", routes[routeIndex][pitchIndex].grade); // save grade of this pitch
					pitchIndex++; // next pitch
				}
			}
			if (routeIndex >= 0 && pitchIndex > 0) {
				routes[routeIndex][0].len = pitchIndex - 1; // save total number of pitch
			}
			fclose(map);
		}
		else { 
			OSTaskDel(20);
			OSTaskDel(30);
		}
		printf("---------------------------------------------------------\n");
		printf("Which route do you climb?\n");
		printf("---------------------------------------------------------\n");
		for (INT8U i = 0; i <= routeIndex; i++) {
			printf("\tRoute %hd. %s (total %hd pitches) :\n", i, routes[i][0].name, routes[i][0].len);
			for (int j = 1; j <= routes[i][0].len; j++) {
				printf("\t\t%hdth pitch : len = %hdm, grade = %s\n", j, routes[i][j].len, routes[i][j].grade);
			}
			printf("\n");
		}
		printf("---------------------------------------------------------\n");
		printf("choose one by number of route.\n\t-> ");
		scanf("%hhd", &route);
		
		printf("route = %s(%hd)\n", routes[route][0].name, route);

		pos = 0;
		total = 0;
		flash = 1;
		OSTaskCreate(Belayer_sign, (void*)0, &TaskStk[1][TASK_STK_SIZE - 1], 20);
		OSTaskCreate(Climber_sign, (void*)0, &TaskStk[2][TASK_STK_SIZE - 1], 30);
		OSTaskSuspend(OS_PRIO_SELF);
	}
}

void Check_rope(void* data) {
	FILE* log;
	INT8 fall_dis = *(INT8U*)data;
	INT8U weight = WEIGHT;
	FP32 accel = sqrt(2 * G * fall_dis); 
	FP32 impact_force = weight * accel * 10; // impact time is 0.1s
	
	if (impact_force >= IMPACT) { 
		rope_drb -= impact_force / IMPACT; // rope's life span is reduced
		printf("rope_durability : %hd\n", rope_drb); 
	}
	if (rope_drb <= 1) { // this rope is now dangerous 
		 // stop climbing
		OSTaskDel(20);
		OSTaskDel(30);

		log = fopen("log.txt", "a");
		printf("Reppel down...\n"); // rappel down immediately.
		fprintf(log, "Reppel down...\n");
		fclose(log);
		OSTimeDly(10);
		while (1);
	}
	else
		OSTaskDel(OS_PRIO_SELF);
}

void Belayer_sign(void* data) {
	INT8U err_sem, err_msg, sleep;
	OS_Q_DATA q_data;
	INT8U msg_out[100];
	void* msg_in;
	FILE* log;
	time_t start, end;

	while (1) {
		OSSemPend(sem, 0, &err_sem); // semaphore
		log = fopen("log.txt", "a"); 
		if (pos >= routes[route][0].len) { // climb all the pitch. arrive top(peak). 
			printf("total time = %hdh %hdm\n", total / 60, total % 60); 
			fprintf(log, "total time = %hdh %hdm\n", total / 60, total % 60); // total climbing time
			if (flash) {
				printf("you flash the route!\n");
				fprintf(log, "you flash the route!\n");
			}
			printf("Reppel down...\n"); // rappel down.
			fprintf(log, "Reppel down...\n");
			fclose(log);
			OSSemPost(sem);
			OSTaskResume(10); 
		}		
		pos++; // next pitch
		time(&start); // to calcurate time taken each pitch
		printf("\n%hd pitch of %s(%hdm, %s)\n", pos, routes[route][0].name, routes[route][pos].len, routes[route][pos].grade); // information of this pitch
		fprintf(log, "\n%hd pitch of %s(%hdm, %s)\n", pos, routes[route][0].name, routes[route][pos].len, routes[route][pos].grade);

		OSTimeDly(1);
		if (rand() % 100 < 50) { // case that sign is not worked
			printf("\tBelayer : ~~~\n"); // there is a lot of noise
			fprintf(log, "\tBelayer : ~~~\n");
			sprintf(msg_out, "~~~"); // incorrect sign
		}
		else {
			printf("\tBelayer : On belay!\n"); // sign is clear
			fprintf(log, "\tBelayer : On belay!\n");
			sprintf(msg_out, "On belay"); // belayer ready to belay
		}
		err_msg = OSQPost(msg_B2C, msg_out); // send a sign
		while (err_msg != OS_NO_ERR) {
			err_msg = OSQPost(msg_B2C, msg_out);
		}
		fclose(log); 
		OSSemPost(sem); // post semaphore to prevent deadlock
		OSTimeDly(1);
		msg_in = OSQPend(msg_C2B, 0, &err_msg); // receive sign from climber
		while (msg_in == 0) { // fail to receive 
			err_msg = OSQPost(msg_B2C, msg_out); // send sign again
			while (err_msg != OS_NO_ERR) {
				err_msg = OSQPost(msg_B2C, msg_out);
			}
		}
		if (strstr(msg_in, "On belay?") != NULL) { // sign does'nt work correctly
			printf("\tBelayer : On belay!\n"); 
			sprintf(msg_out, "On belay"); // belayer ready to belay
			err_msg = OSQPost(msg_B2C, msg_out); // send a sign again
			while (err_msg != OS_NO_ERR) {
				err_msg = OSQPost(msg_B2C, msg_out);
			}
			msg_in = OSQPend(msg_C2B, 0, &err_msg); // receive sign from climber
		}

		OSSemPend(sem, 0, &err_sem);
		log = fopen("log.txt", "a");
		if (strstr(msg_in, "Safe") != NULL) { // safe sign from climber
			time(&end); 
			OSTimeDly(1);
			printf("\tBelayer : Off belay!\n"); // off belay and ready to follow
			fprintf(log, "\tBelayer : Off belay!\n");
			printf("time = %lldm\n", (end - start) * 4);
			fprintf(log, "time = %lldm\n", (end - start) * 4);
			total += (end - start) * 4; 
		}
		fclose(log);
		OSSemPost(sem);
	}
}

void Climber_sign(void* pdata) {
	INT8U err_sem, err_msg;
	OS_Q_DATA q_data;
	INT8 msg_out[100];
	void* msg_in;
	FILE* log;

	while (1) {
		msg_in = OSQPend(msg_B2C, 0, &err_msg);
		while (strstr(msg_in, "On belay") == NULL) { // wait : haven't received a sign from belayer
			printf("\tClimber : On belay?\n");
			sprintf(msg_out, "On belay?\n"); // check belayer is ready or not
			err_msg = OSQPost(msg_C2B, msg_out); // send a sign
			while (err_msg != OS_NO_ERR) {
				err_msg = OSQPost(msg_C2B, msg_out);
			}
			msg_in = OSQPend(msg_B2C, 0, &err_msg); // receive sign
		}
		OSTimeDly(1);

		OSSemPend(sem, 0, &err_sem);
		log = fopen("log.txt", "a");
		printf("\tClimber : Climbing!\n"); 
		fprintf(log, "\tClimber : Climbing!\n");

		INT8U pace = 15; // how fast in this route : depends on the grade
		// 5.7 -> 5.8 -> 5.9 -> 5.10a -> 5.10b .. 5.10d -> 5.11a ... 5.11d ... 5.14d .. 
		// ^ (more close to the right side, it is indicate more hard and difficult)	
		if (strstr(routes[route][pos].grade, "10") != NULL) {
			pace = 10;
		}
		else if (strstr(routes[route][pos].grade, "11") != NULL) {
			pace = 5;
		}
		INT8 height = 0; // current height of climber : measured by altitude sensor
		INT8 fall_dis = 0; // fall distance
		INT8U fall_prb = 20 - pace; // probability of falling

		while (height < routes[route][pos].len) { // climbing start!
			height += (rand() % pace); // climbing distance in one unit time
			height = (height > routes[route][pos].len) ? routes[route][pos].len : height; // climber can't go above the end of pitch
			fall_dis = (height - fall_dis) * 2; // fall distance = current height - (distance from last protection * 2) 
			printf("\t\tcurrent height : %hd/%hd\n", height, routes[route][pos].len);
			fprintf(log, "\t\tcurrent height : %hd/%hd\n", height, routes[route][pos].len);
			if (rand() % 60 < fall_prb && height != routes[route][pos].len) { // if climber fall
				flash = 0;
				height -= fall_dis;
				fclose(log);
				OSSemPost(sem); // to prevent deadlock
				OSTaskCreate(Check_rope, (void*)&fall_dis, &TaskStk[4][TASK_STK_SIZE - 1], 5); // check the rope is fine or not
				OSSemPend(sem, 0, &err_sem);
				log = fopen("log.txt", "a");
				if (height > 0) { // fall but does not touch the ground -> continue to climb
					printf("\t\t\tclimber fall!\n");
					fprintf(log, "\t\t\tclimber fall!\n");
					printf("\t\tcurrent height : %hd, fall distance = %hd\n", height, fall_dis);
					fprintf(log, "\t\tcurrent height : %hd, fall distance = %hd\n", height, fall_dis);
				}
				else { // climber touch the ground : get injured
					printf("Emergency\n");
					fprintf(log, "Emergency\n");
					fclose(log);
					OSSemPost(sem);
					fall_dis += height;
					OSTaskCreate(Rescue_request, (void*)&fall_dis, &TaskStk[3][TASK_STK_SIZE - 1], 0); // request a rescue
				}
			}
			fall_dis = height; // reset the fall_dis to the position set last protection
			OSTimeDly(1);
		}
		printf("\tClimber : Safe!\n"); // complete to climb one pitch
		fprintf(log, "\tClimber : Safe!\n");
		sprintf(msg_out, "Safe\n");
		err_msg = OSQPost(msg_C2B, msg_out); // send a safe sign to belayer
		while (err_msg != OS_NO_ERR) {
			err_msg = OSQPost(msg_C2B, msg_out);
		}
		fclose(log);
		OSSemPost(sem);
		OSTimeDly(1);
	}
}

void Rescue_request(void* pdata) {
	FILE* emergency_contact = fopen("source/emergency_contact.txt", "r");
	INT8 buf[BUFSIZE];
	INT8 help_msg[BUFSIZE]; // message about where are we, blood type, fall distance(patient's condition)
	INT8 fall_dis = *(INT8*)pdata; // fall distance
	INT8 blood_type[3] = BLOODTYPE;

	OSTaskDel(10);
	OSTaskDel(20);
	OSTaskDel(30);

	sprintf(help_msg, "\tcurrent_location is Bukhansan-Mountain, Insubong-peak, %s route, %hd pitch\n"
						"\tblood_type is %s\n\tfall_distance is %hdm\n", routes[route][0].name, pos, blood_type, fall_dis); 
	while (fgets(buf, BUFSIZE, emergency_contact) != NULL) { 
		printf("send message to %s%s", buf, help_msg); // send a help message every emergenct_contact
	}
	fclose(emergency_contact);
	
	while (1);
}
