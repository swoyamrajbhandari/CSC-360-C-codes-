Differences(p2a):

Q1) I didnt create controller threads. I used 2 functions nextStation() and trainDispatch() for coordinating the order in which trains cross the main track based on the rules specified.

Q3) I didnt use direction and priority mutexes and they seemed excessive and unnecessary.Similary no starvation mutex, as it was handeled by global variables.

Q4) The main thread wont handle control system threads anymore since I didnt create them. Instead it calls trainDispatch() to do the same work.

Q7) The conditional variable for control system decisions was replaced by the conditional variables for each train(train_convar). Its associated mutex was the mutex that locks the track section(Main track mutex).  

Rather than a conditional variable that represents the condition where the main track, I used a convar (crossed) that signals a train has finished crossing hence indirectly showing that the track is empty.

Brief overview of my code.
=> The code reads an input file to determine the number of trains and their attributes and stores this information in an array of train structures.
Mutexes and condition variables are initialized. Threads are created, one for each train. The start_train function is called to signal that all trains are ready to load and starts a timer. Each train thread waits for a signal to start loading, simulates loading time, and then moves to a station based on its direction (east or west).Trains are enqueued in the respective station lists according to priority rules.The trainDispatch function decides which train to dispatch next based on station queues and history. It signals the chosen train to cross the track and updates the station queue accordingly.Train threads wait for a signal to cross the track, simulate crossing time, and signal when they have completed the crossing. The code maintains time information using clock_gettime and prints the time when trains start loading, enter the track, and leave the track.The main loop continues until all trains have crossed the track, at which point the code proceeds to cleanup and terminate.



