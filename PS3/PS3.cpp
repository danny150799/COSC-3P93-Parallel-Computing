# include <bits/stdc++.h>
# include "mpi.h"
# include <string.h>
# include <time.h>
using namespace std;
// compile command: mpic++ -g -Wall -o test PS3.cpp
// execute command: mpiexec -n <desired # of processors> ./test
const string GENES = "abcdefghijklmnopqrstuvwsyzABDCDEFGHIJKLMNOPQRSTUVWSYZ"\
"1234567890,./<>? ;':[]{}!@#$%^&*()_+-=";
string target = "Hello world.Hello world.Hello world.Hello world.Hello world.Hello world.Hello world.Hello world.Hello world.Hello world.";
int size = 1024;
int pop_size = 1000;

/**
 * This function generates a random integer in the range of the given min and max.
 * 
 * **/
int random(int str, int end) { 
    int range = (end-str)+1; 
    int ran = str + (rand()%range);
    return ran; 
} 

/**
 * This function returns a random characters in the GENES pool.
 * 
 * **/
char mutation(){
	int rand = random(0, GENES.size()-1); 
	return GENES[rand];
}

/**
 * This function creates a  chromosome by randomly picking a characters from the GENES pool adds them together.
 * 
 * **/
string populate(int size){
	string new_chromosome = "";
	for(int i=0; i<size; i++){
		new_chromosome += mutation();
	}
	return new_chromosome;
}

/**
 * This class represents the gene in which it contains a string as chromosome and an int as fitness score.
 * 
 * **/
class gene{
public:
	string chromosome;
	int fitness;
	int get_fitness();
	gene(string chromosome);
	gene create_offspring(gene partner);
};

/**
 * This function initializes a gene when it is created.
 * 
 * **/
gene::gene (string chromosome){
	this->chromosome = chromosome;
	fitness = get_fitness();
}

/**
 * This function calculates the fitness score for a chromosome by matching it with the target string.
 * 
 * **/
int gene::get_fitness(){
	int score = 0;
	int size = chromosome.size();
	for(int i=0; i<size; i++){
		if(chromosome[i] == target[i]){
			score += 1;
		}
	}
	return score;
}

/**
 * This function manages the sort method of vector. high to low. (highest in the front)
 * 
 * **/
bool operator<(const gene gene1, const gene gene2) { 
    return gene1.fitness > gene2.fitness; 
} 

/**
 * This function creates a new chromosome by taking a character from parent1 or parent2 based on the random number or mutate the gene with random characters.
 * rand is the randome num,  	 rand <0.5 -> take chromosome of parent1
 * 							0.5< rand <0.9 -> take chromosome of parent2
 * 							0.9< rand <1	 -> mutation: random characters from GENES pool
 * 
 * **/
gene gene::create_offspring(gene partner){
	string child = "";

	int size = chromosome.size();
	for(int i=0; i<size; i++){
		int rand = random(0, 100)/100;
		if (rand < 0.5){
			child += chromosome[i];
		}else if(rand < 0.9){
			child += partner.chromosome[i];
		}else{
			child += mutation();
		}
	}
	return gene (child);
}

/**
 * This function removes the last element from the vector until the size of vector is equal to the given integer.
 * 
 * **/
vector<gene> correct_vector_size(vector<gene> pop, int pop_size){
	while((unsigned)pop.size()>(unsigned)pop_size){
		pop.erase(pop.end());
	}
	return pop;
}

int main(){
	clock_t start = clock();							// initalize vairables
	int rank, comm_size;

	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	srand ((unsigned)1);
	int generation = 0;
	vector<gene> population;
	int result = 0;
	string new_pop;						
	char characters[size];								// initalize vairables
	int partition = ceil(pop_size/(double)(comm_size-1)); 	// partition is calculate so that the processors give the exact # of population or more.
															// see report for more details
	
	if (rank != 0) { // slave processors
		for(int i=0; i< partition; i++){					// create random chromosomes and send it to the master processor.
			new_pop = populate(target.size());
			sprintf(characters, "%s", new_pop.c_str());
			MPI_Send(characters, strlen(characters)+1, MPI_CHAR, 0, 0, MPI_COMM_WORLD); 
		}

		string temp[2];
		int num_mate = (90 * pop_size)/100;
		partition = ceil(num_mate/(double)comm_size-1);		// partition is adjusted to 90% of population because elitism already handle 10%.

		while(result ==0){									
			for(int p=0; p<partition; p++){					// start receive parents and create offsrpings and send to master processor
				for(int i=0; i<2; i++){
					MPI_Recv(characters, size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	//receive parents' chromosomes
					temp[i] = characters;
				}
				gene parent1 = gene(temp[0]);
				gene parent2 = gene(temp[1]);
				gene child = parent1.create_offspring(parent2);				
				
				sprintf(characters, "%s", child.chromosome.c_str());
				MPI_Send(characters, strlen(characters)+1, MPI_CHAR, 0, 0, MPI_COMM_WORLD); 
			}
		}
	} else {  // master processor
		for(int i=0; i< partition; i++){					// receives random chromosomes 
			for (int q = 1; q < comm_size; q++) {
				MPI_Recv(characters, size, MPI_CHAR, q, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				population.push_back(gene(characters));
			} 
		}
		population = correct_vector_size(population, pop_size);	// correct the vector size
	
		while(result==0){
			sort(population.begin(), population.end());			// sort from high to low
		
			if((unsigned)population[0].fitness == target.size()){		// break if found the solution
				result = 1;
				break;
			}
			vector<gene> new_gen;

			int elite = (10 * pop_size)/100;					// perfrom elitism on top 10% of population
			for(int i=0; i<elite; i++){
				new_gen.push_back(population[i]);
			}

			int num_mate = (90 * pop_size)/100;
			partition = ceil(num_mate/(double)comm_size-1);

			for(int p=0; p<partition; p++){						// send parents' chromosomes and receive offspring chromosome
				for(int j=0; j<2; j++){
					for(int i=1; i<comm_size; i++){
						int rand = random(0, num_mate);
						string temp = population[rand].chromosome;
						sprintf(characters, "%s", temp.c_str());
						MPI_Send(characters, strlen(characters)+1, MPI_CHAR, i, 0, MPI_COMM_WORLD); 
					}
				}
				for (int q = 1; q < comm_size; q++) {
					MPI_Recv(characters, size, MPI_CHAR, q, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					new_gen.push_back(gene(characters));
				} 
			}
			sort(population.begin(), population.end());
			population = new_gen; /**													// uncomment to print the fittest in each generation
			cout << "Generation: " << generation << "\t"; 
			cout << "Chromosome: " << population[0].chromosome <<"\t"; 
			cout << "Fitness score: " << population[0].fitness << "\n";
			**/
			generation++;
		}
		cout << "Generation: " << generation << "\t"; 
		cout << "Chromosome: " << population[0].chromosome <<"\t"; 
		cout << "Fitness score: " << population[0].fitness << "\n";
		printf("Execution time is: %.2fs\n", (double)(clock()-start)/CLOCKS_PER_SEC);
	}
    MPI_Abort(MPI_COMM_WORLD, 1);														// terminate all processes
	
    MPI_Finalize();
	return 0;
}