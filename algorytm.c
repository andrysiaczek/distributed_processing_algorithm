#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#define REQUEST 0
#define CONSENT 1
#define RELEASE 2
#define CRITICAL 3
#define BLOCK 4
#define EMPTY -1
#define MALE 0
#define FEMALE 1
#define FALSE 0
#define TRUE 1


int main( int argc, char *argv[] )
{
    int priority = 0; // the priority of the process
    int L[3] = { 0 }; // the number of occupied lockers in each locker room
    int T[3] = { EMPTY, EMPTY, EMPTY }; // the type of each locker room (-1)-empty 0-male 1-female
	int B[3] = { FALSE }; // if the locker rooms are blocked 0-false 1-true
	char *arg = argv[1];
  	int M = atoi(arg); // the number of lockers in each locker room
	int rank; // task id (rank of the process)
	int numtasks; // the number of all tasks
	char processor_name[MPI_MAX_PROCESSOR_NAME]; // the buffer for the processor name
	int proc_namelen; // the length of the processor name
	int request[3], consent, critical[2], release, block[2]; // buffers for different types of messages
	int message[3]; // buffer to receive message with unknown tag
	int try_critical = FALSE; // set try to get to critical section to 0 - false
	int chosen_locker = -1; // the number of the locker room the process is trying to access
	int num_consents = -1; // the number of required consents to enter the critical section

	MPI_Init( &argc, &argv ); // initializes the MPI execution environment
	
	MPI_Comm_rank( MPI_COMM_WORLD, &rank ); // get the rank of the calling MPI process
	MPI_Comm_size( MPI_COMM_WORLD, &numtasks ); // get the number of available MPI tasks
	MPI_Get_processor_name(processor_name,&proc_namelen); // the processor name and its length

	MPI_Request reqs_send[numtasks-1]; // the request number of the message
	MPI_Request reqs_rec[numtasks-1]; // the request number of the message
	MPI_Status status_send[numtasks-1]; // the status of the message
	MPI_Status status_rec[numtasks-1]; // the status of the message
	MPI_Status status; // the status of the message

	int flag; // the flag of the MPI_Iprobe test for a message
	srand (time(NULL) + rank);
    int sex = rand() % 2; // the sex of the process 0-male 1-female

	int *consent_queue = (int *)malloc(sizeof(int)*numtasks); // the queue of overdue consents
	for (int i = 0; i < numtasks; i++){ // initialize the consent_queue values with 0
		consent_queue[i] = 0;
	}

	printf("%d: Moja płeć: %d.\n", rank, sex);
	// printf("%d: Liczba miejsc w szatni: %d, a liczba procesow: %d\n", rank, M, numtasks); #TODO remove line
	while (1)
	{
		if (!try_critical && rand() % 4 == 0) // 25% chance to try to enter critical section
		{
			try_critical = TRUE; // from now on the process tries to get to critical section

			for (int i = 0; i < sizeof(L)/sizeof(int); i++)  // iterate over locker rooms
			{
				if (T[i] == sex && L[i] != M) // if the same sex and not full
				{
					chosen_locker = i;
					break;
				}
				else if (T[i] == sex && chosen_locker == -1) // if the same sex and full
				{
					chosen_locker = i;
				}
				else if (T[i] == EMPTY && chosen_locker == -1) // if empty
				{ 
					chosen_locker = i;
				}
			}

			if (chosen_locker == -1) // there are no locker rooms with corresponding sex
			{ 
				chosen_locker = 0;
				int min_number = L[0];
				for (int i = 1; i < sizeof(L)/sizeof(int); i++) // choose the locker room to be blocked
				{
					if (L[i] < min_number)
					{
						chosen_locker = i;
						min_number = L[i];
					}
				}
				for (int i = 0; i < numtasks; i++) // send block message to other processes
				{
					int minus = 0;
					if (i != rank) // if not itself
					{
						block[0] = ++priority;
						block[1] = chosen_locker;
						// MPI_Isend(&block, 2, MPI_INT, i, BLOCK, MPI_COMM_WORLD, &reqs_send[i-minus]);
						MPI_Send(&block, 2, MPI_INT, i, BLOCK, MPI_COMM_WORLD);
						printf("%d: Blokuję szatnię %d - info dla procesu: %d.\n", rank, chosen_locker, i);
					}
					else
					{
						minus = 1;
					}
				}
				// printf("%d: Zablokowałem szatnię nr: %d. Moja płeć to: %d\n", rank, chosen_locker, sex); // #TODO remove line
				// MPI_Waitall(numtasks-1, reqs_send, status_send);
				num_consents = numtasks-1;
			}
			else
			{
				for (int i = 0; i < numtasks; i++) // send request message to other processes
				{
					int minus = 0;
					if (i != rank) // if not itself
					{
						request[0] = ++priority;
						request[1] = chosen_locker;
						request[2] = sex;
						// MPI_Isend(&request, 3, MPI_INT, i, REQUEST, MPI_COMM_WORLD, &reqs_send[i-minus]);
						MPI_Send(&request, 3, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
						printf("%d: Wysyłam prośbę o wejście do szatni %d do procesu: %d.\n", rank, chosen_locker, i);
					}
					else
					{
						minus = 1;
					}
				}
				printf("%d: Próbuję dostać się do szatni: %d o typie: %d.\n", rank, chosen_locker, T[chosen_locker]);

				if (B[chosen_locker] == 1 || T[chosen_locker] == EMPTY){
					num_consents = numtasks-1;
				}
				else
				{
					num_consents = numtasks - (M - L[chosen_locker]);
				}
				// printf("%d: Czekam na dostarczenie moich żądań o wejście do szatni: %d.\n", rank, chosen_locker); #TODO remove line
				// MPI_Waitall(numtasks-1, reqs_send, status_send); // #TODO leave or remove?
			}
		}

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status); // check if there is a message for the process to be received
		// printf("%d: Czekam na info od innych.\n", rank); #TODO remove line
		if (flag)
		{
			MPI_Recv(&message, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		}
		else
		{
			continue;
		}
		
		if (status.MPI_TAG == REQUEST) // request message received
		{
			printf("%d: Dostałem żądanie od procesu %d.\n", rank, status.MPI_SOURCE);
			
			int process_priority = message[0];
			int process_locker_number = message[1];
			int process_sex = message[2];

			if (process_locker_number == chosen_locker)
			{
				if (process_priority > priority || (process_priority == priority && status.MPI_SOURCE > rank))
				{
					MPI_Send(&consent, 0, MPI_INT, status.MPI_SOURCE, CONSENT, MPI_COMM_WORLD); // sending back consent
					printf("%d: Odsyłam zgodę procesowi: %d na wejście do szatni %d.\n", rank, status.MPI_SOURCE, process_locker_number);
					if (process_sex != sex) // if two processes with different sex want to enter the same locker room
					{
						printf("Process sex: %d.\n", process_sex);
						T[chosen_locker] = process_sex;
						try_critical = FALSE;
						chosen_locker = -1;
					}
				}
				else
				{
					consent_queue[status.MPI_SOURCE] = TRUE;
					printf("%d: Dopisuję proces %d do mojej oczekujacej listy.\n", rank, status.MPI_SOURCE);
				}
			}
			else
			{
				printf("Process sex: %d.\n", process_sex);
				T[process_locker_number] = process_sex;
				MPI_Send(&consent, 0, MPI_INT, status.MPI_SOURCE, CONSENT, MPI_COMM_WORLD); // sending back consent
				printf("%d: Odsyłam zgodę procesowi: %d na wejście do szatni %d.\n", rank, status.MPI_SOURCE, process_locker_number);
			}
		}
		else if (status.MPI_TAG == CONSENT) // consent message received
		{
			printf("%d: Dostałem zgodę od procesu %d.\n", rank, status.MPI_SOURCE);

			if (try_critical)
			{
				num_consents -= 1;
				priority += 1;
				if (num_consents == 0)
				{
					// critical section
					for (int i = 0; i < numtasks; i++) // send critical message to other processes
					{
						int minus = 0;
						if (i != rank) // if not itself
						{
							critical[0] = sex;
							critical[1] = chosen_locker;
							// MPI_Isend(&critical, 2, MPI_INT, i, CRITICAL, MPI_COMM_WORLD, &reqs_send[i-minus]);
							MPI_Send(&critical, 2, MPI_INT, i, CRITICAL, MPI_COMM_WORLD);
							printf("%d: Wysyłam informację o wejściu do sekcji krytycznej w szatni %d procesowi: %d.\n", rank, chosen_locker, i);
						}
						else
						{
							minus = 1;
						}
					}
					printf("%d: Wszedłem do sekcji krytycznej w szatni: %d\n", rank, chosen_locker);
					// MPI_Waitall(numtasks-1, reqs_send, status_send);

					// #TODO czy wysylanie zgod dobrze zaimplementowane?
					int count = 0;
					for (int i = 0; i < numtasks; i++)
					{
						if (consent_queue[i]){
							// MPI_Isend(&consent, 0, MPI_INT, i, CONSENT, MPI_COMM_WORLD, &reqs_send[count]); // sending back overdue consent
							MPI_Send(&consent, 0, MPI_INT, i, CONSENT, MPI_COMM_WORLD); // sending back overdue consent
							printf("%d: Rozsyłam zaległa zgodę dla procesu: %d.\n", rank, i);
							count++;
						}
					}
					// MPI_Waitall(count+1, reqs_send, status_send);

					sleep(rand() % 21); // sleep no longer than 20 seconds #TODO sleep function can't be used in critical section

					for (int i = 0; i < numtasks; i++) // send release message to other processes
					{
						int minus = 0;
						if (i != rank) // if not itself
						{
							release = chosen_locker;
							// MPI_Isend(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD, &reqs_send[i-minus]);
							MPI_Send(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
							printf("%d: Wysyłam informację o zwolnieniu zasobów w szatni %d do procesu: %d.\n", rank, chosen_locker, i);
						}
						else
						{
							minus = 1;
						}
					}
					printf("%d: Wyszłam z sekcji krytycznej w szatni: %d\n", rank, chosen_locker);
					try_critical = FALSE;
					priority = 0;
					// MPI_Waitall(numtasks-1, reqs_send, status_send);
				}
			}
		}
		else if (status.MPI_TAG == RELEASE) // release message received
		{
			printf("%d: Dostałem informację o zwolnieniu zasobów od procesu %d.\n", rank, status.MPI_SOURCE);

			int process_locker_number = message[0];
			L[process_locker_number]--;
			if (L[process_locker_number] == 0 && !B[process_locker_number])
			{
				T[process_locker_number] = EMPTY;
			}
		}
		else if (status.MPI_TAG == CRITICAL) // critical message received
		{
			printf("%d: Dostałem informację o wejsciu do sekcji krytycznej do szatni nr %d od procesu %d.\n", rank, message[1], status.MPI_SOURCE);

			int process_sex = message[0];
			int process_locker_number = message[1];

			L[process_locker_number]++;
			if (L[process_locker_number] > M){ // #TODO usunac - potrzebne tylko do debuggowania
				printf("%d: Szatnia: %d przepełniona. Więcej klientów niż dostępnych szafek. Ostatni wchodzący proces: %d.\n", rank, process_locker_number, status.MPI_SOURCE);
			}
			if (T[process_locker_number] != process_sex && T[process_locker_number] != EMPTY) // #TODO usunac - potrzebne tylko do debuggowania
			{
				printf("%d: Przemieszanie płci w szatni: %d, która jest szatnią %d. Ostatni wchodzący proces: %d ma płeć %d.\n", rank, process_locker_number, T[process_locker_number], status.MPI_SOURCE, process_sex);
			}
			if (T[process_locker_number] == EMPTY)
			{
				printf("Process critical sex: %d.\n", process_sex);
				T[process_locker_number] = process_sex;
			}
			else if (B[process_locker_number] && process_sex == T[process_locker_number])
			{
				B[process_locker_number] = FALSE;
			}

			consent_queue[status.MPI_SOURCE] = FALSE;
		}
		else if (status.MPI_TAG == BLOCK) // block message received
		{
			printf("%d: Dostałem informację o zablokowaniu szatni nr %d od procesu %d.\n", rank, message[1], status.MPI_SOURCE);

			int process_priority = message[0];
			int process_locker_number = message[1];

			if (process_locker_number == chosen_locker)
			{
				consent_queue[status.MPI_SOURCE] = TRUE;
			}
			else
			{
				MPI_Send(&consent, 0, MPI_INT, status.MPI_SOURCE, CONSENT, MPI_COMM_WORLD); // sending back consent
				printf("%d: Wysyłam zgodę procesowi %d, ktory zablokował szatnię %d.\n", rank, status.MPI_SOURCE, process_locker_number);
			}

			B[process_locker_number] = TRUE;
			T[process_locker_number] = (T[process_locker_number] + 1) % 2; // change if female to male, if male to female
		}	
	}

	printf("byeee\n");
	MPI_Finalize(); // terminates the MPI execution environment
}

// // blokujace
// partner = numtasks/2 + taskid;
// MPI_Send(&taskid, 1, MPI_INT, partner, 1, MPI_COMM_WORLD);
// MPI_Recv(&message, 1, MPI_INT, partner, 1, MPI_COMM_WORLD, &status);

// // nieblokujace
// MPI_Irecv(&message, 1, MPI_INT, partner, 1, MPI_COMM_WORLD, &reqs[0]);
// MPI_Isend(&taskid, 1, MPI_INT, partner, 1, MPI_COMM_WORLD, &reqs[1]);
// /* now block until requests are complete */
// MPI_Waitall(2, reqs, stats);