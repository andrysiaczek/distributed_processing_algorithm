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

void critical_section(int *critical_count, int tasks, int rank, int chosen_locker)
{
	*critical_count -= 1; // decrease critical section counter by 1 in each iteration

	if (!*critical_count) // if critical_count equals 0
	{
		for (int i = 0; i < tasks; i++) // send release message to other processes
		{
			int minus = 0;
			if (i != rank) // if not itself
			{
				int release = chosen_locker;
				// MPI_Isend(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD, &reqs_send[i-minus]); // #TODO remove line
				MPI_Send(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
				printf("%d: Wysyłam informację o zwolnieniu zasobów w szatni %d do procesu: %d.\n", rank, chosen_locker, i);
			}
			else
			{
				minus = 1;
			}
		}
		printf("%d: Wyszedłem z sekcji krytycznej w szatni: %d\n", rank, chosen_locker);
		
		// MPI_Waitall(tasks-1, reqs_send, status_send); // #TODO remove line
	}
}

void request_message_received(int rank, int source_process, int *message, int *chosen_locker, int priority, int sex, int *T, int *try_critical, int *overdue_consents)
{
	printf("%d: Dostałem żądanie od procesu %d.\n", rank, source_process);
			
	int process_priority = message[0];
	int process_locker_number = message[1];
	int process_sex = message[2];

	if (process_locker_number == *chosen_locker)
	{
		if (process_priority > priority || (process_priority == priority && source_process > rank))
		{
			int consent;
			MPI_Send(&consent, 0, MPI_INT, source_process, CONSENT, MPI_COMM_WORLD); // sending back consent
			printf("%d: Odsyłam zgodę procesowi: %d na wejście do szatni %d.\n", rank, source_process, process_locker_number);

			if (process_sex != sex) // if two processes with different sex want to enter the same locker room
			{
				T[*chosen_locker] = process_sex;
				*try_critical = FALSE;
				*chosen_locker = -1;
			}
		}
		else
		{
			overdue_consents[source_process] = TRUE;
			printf("%d: Dopisuję proces %d do mojej oczekujacej listy.\n", rank, source_process);
		}
	}
	else
	{
		T[process_locker_number] = process_sex;
		int consent;
		MPI_Send(&consent, 0, MPI_INT, source_process, CONSENT, MPI_COMM_WORLD); // sending back consent
		printf("%d: Odsyłam zgodę procesowi: %d na wejście do szatni %d.\n", rank, source_process, process_locker_number);
	}
}

void consent_message_received(int rank, int source_process, int *try_critical, int *num_consents, int *priority, int tasks, int sex, int chosen_locker, int *overdue_consents, int *critical_count)
{
	printf("%d: Dostałem zgodę od procesu %d.\n", rank, source_process);

	if (*try_critical)
	{
		*num_consents -= 1;
		*priority += 1;
		if (*num_consents == 0)
		{
			// critical section
			for (int i = 0; i < tasks; i++) // send critical message to other processes
			{
				int minus = 0;
				if (i != rank) // if not itself
				{
					int critical[2];
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
			// MPI_Waitall(tasks-1, reqs_send, status_send);

			// #TODO czy wysylanie zgod dobrze zaimplementowane?
			int count = 0;
			for (int i = 0; i < tasks; i++)
			{
				if (overdue_consents[i]){
					int consent;
					// MPI_Isend(&consent, 0, MPI_INT, i, CONSENT, MPI_COMM_WORLD, &reqs_send[count]); // sending back overdue consent
					MPI_Send(&consent, 0, MPI_INT, i, CONSENT, MPI_COMM_WORLD); // sending back overdue consent
					printf("%d: Rozsyłam zaległa zgodę dla procesu: %d.\n", rank, i);
					count++;
				}
			}
			// MPI_Waitall(count+1, reqs_send, status_send);

			// sleep(rand() % 21); // sleep no longer than 20 seconds #TODO sleep function can't be used in critical section // #TODO remove line
			*critical_count = 100;
			*try_critical = FALSE;
			*priority = 0;
		}
	}
}

void release_message_received(int rank, int source_process, int process_locker_number, int *L, int *B, int *T)
{
	printf("%d: Dostałem informację o zwolnieniu zasobów od procesu %d.\n", rank, source_process);

	L[process_locker_number] -= 1; // decrease number of occupied lockers

	if (L[process_locker_number] == 0 && !B[process_locker_number]) // if locker room empty and not blocked
	{
		T[process_locker_number] = EMPTY;
	}
}

void critical_message_received(int rank, int source_process, int *message, int *L, int M, int *T, int *B, int *overdue_consents)
{
	printf("%d: Dostałem informację o wejsciu do sekcji krytycznej do szatni nr %d od procesu %d.\n", rank, message[1], source_process);

	int process_sex = message[0];
	int process_locker_number = message[1];

	L[process_locker_number] += 1;
	if (L[process_locker_number] > M){ // #TODO usunac - potrzebne tylko do debuggowania
		printf("%d: Szatnia: %d przepełniona. Więcej klientów niż dostępnych szafek. Ostatni wchodzący proces: %d.\n", rank, process_locker_number, source_process);
	}
	if (T[process_locker_number] != process_sex && T[process_locker_number] != EMPTY) // #TODO usunac - potrzebne tylko do debuggowania
	{
		printf("%d: Przemieszanie płci w szatni: %d, która jest szatnią %d. Ostatni wchodzący proces: %d ma płeć %d.\n", rank, process_locker_number, T[process_locker_number], source_process, process_sex);
	}
	if (T[process_locker_number] == EMPTY)
	{
		T[process_locker_number] = process_sex;
	}
	else if (B[process_locker_number] && process_sex == T[process_locker_number])
	{
		B[process_locker_number] = FALSE;
	}

	overdue_consents[source_process] = FALSE;
}

void block_message_received(int rank, int source_process, int process_priority, int process_locker_number, int chosen_locker, int *overdue_consents, int *B, int *T)
{
	printf("%d: Dostałem informację o zablokowaniu szatni nr %d od procesu %d.\n", rank, process_locker_number, source_process);

	if (process_locker_number == chosen_locker)
	{
		overdue_consents[source_process] = TRUE;
	}
	else
	{
		int consent;
		MPI_Send(&consent, 0, MPI_INT, source_process, CONSENT, MPI_COMM_WORLD); // sending back consent
		printf("%d: Wysyłam zgodę procesowi %d, ktory zablokował szatnię %d.\n", rank, source_process, process_locker_number);
	}

	B[process_locker_number] = TRUE;
	T[process_locker_number] = (T[process_locker_number] + 1) % 2; // change if female to male, if male to female
}

int main( int argc, char *argv[] )
{
    int priority = 0; // the priority of the process
    int L[3] = { 0 }; // the number of occupied lockers in each locker room
    int T[3] = { EMPTY, EMPTY, EMPTY }; // the type of each locker room [empty/male/female]
	int B[3] = { FALSE }; // if the locker rooms are blocked
	char *arg = argv[1];
  	int M = atoi(arg); // the number of lockers in each locker room, command line argument
	int rank; // task id (rank of the process)
	int tasks; // the number of all tasks (processes) running in MPI execution environment
	int request[3], consent, critical[2], release, block[2]; // buffers for different types of messages
	int message[3]; // buffer to receive message with unknown tag
	int try_critical = FALSE; // initialize variable 'try to get to the critical section' to FALSE
	int critical_count = 0; // how many iterations left to leave the critical section
	int chosen_locker = -1; // the number of the locker room the process is trying to access
	int num_consents = -1; // the number of required consents to enter the critical section

	MPI_Init( &argc, &argv ); // initializes the MPI execution environment
	
	MPI_Comm_rank( MPI_COMM_WORLD, &rank ); // get the rank of the calling MPI process
	MPI_Comm_size( MPI_COMM_WORLD, &tasks ); // get the number of available MPI tasks

	MPI_Request reqs_send[tasks-1]; // the request number of the message // #TODO remove with non-blocking communication
	MPI_Request reqs_rec[tasks-1]; // the request number of the message // #TODO remove with non-blocking communication
	MPI_Status status_send[tasks-1]; // the status of the message // #TODO remove with non-blocking communication
	MPI_Status status_rec[tasks-1]; // the status of the message // #TODO remove with non-blocking communication
	MPI_Status status; // the status of the message

	int flag; // the flag of the MPI_Iprobe test for a message
	srand (time(NULL) + rank); // seed the random generator differently for each process
    int sex = rand() % 2; // the pseudo-randomly chosen sex of the process 0-MALE 1-FEMALE

	int *overdue_consents = (int *)malloc(sizeof(int)*tasks); // the queue of overdue consents
	for (int i = 0; i < tasks; i++){ // initialize the overdue_consents values with 0
		overdue_consents[i] = 0;
	}

	printf("%d: Moja płeć: %d.\n", rank, sex);
	// printf("%d: Liczba miejsc w szatni: %d, a liczba procesow: %d\n", rank, M, tasks); #TODO remove line
	while (1)
	{
		if (critical_count > 0) // if process in the critical section
		{
			critical_section(&critical_count, tasks, rank, chosen_locker); // execute critical section code
		}
		else if (!try_critical && rand() % 4 == 0) // 25% chance to try to enter critical section
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
				for (int i = 0; i < tasks; i++) // send block message to other processes
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
				// MPI_Waitall(tasks-1, reqs_send, status_send);
				num_consents = tasks-1;
			}
			else
			{
				for (int i = 0; i < tasks; i++) // send request message to other processes
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
					num_consents = tasks-1;
				}
				else
				{
					num_consents = tasks - (M - L[chosen_locker]);
				}
				// printf("%d: Czekam na dostarczenie moich żądań o wejście do szatni: %d.\n", rank, chosen_locker); #TODO remove line
				// MPI_Waitall(tasks-1, reqs_send, status_send); // #TODO leave or remove?
			}
		}

		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status); // check if there is a message for the process to be received

		if (flag)
		{
			MPI_Recv(&message, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		}
		else
		{
			continue;
		}
		
		switch (status.MPI_TAG) // take the appropriate action depending on the message tag
		{
		case REQUEST:
			request_message_received(rank, status.MPI_SOURCE, message, &chosen_locker, priority, sex, T, &try_critical, overdue_consents);
			break;

		case CONSENT:
			consent_message_received(rank, status.MPI_SOURCE, &try_critical, &num_consents, &priority, tasks, sex, chosen_locker, overdue_consents, &critical_count);	
			break;
		
		case RELEASE:
			release_message_received(rank, status.MPI_SOURCE, message[0], L, B, T);
			break;

		case CRITICAL:
			critical_message_received(rank, status.MPI_SOURCE, message, L, M, T, B, overdue_consents);
			break;

		case BLOCK:
			block_message_received(rank, status.MPI_SOURCE, message[0], message[1], chosen_locker, overdue_consents, B, T);
			break;
		
		default:
			break;
		}
	}
	MPI_Finalize(); // terminates the MPI execution environment
}