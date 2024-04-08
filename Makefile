all: process.c mmu.c Master.c sched.c
	gcc Master.c -o Master -lm
	gcc sched.c -o scheduler -lm
	gcc mmu.c -o mmu -lm
	gcc process.c -o process -lm

clean:
	rm process Master scheduler mmu