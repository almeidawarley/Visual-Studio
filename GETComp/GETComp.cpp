/**
	Information Diffusion in Complex Networks
	GETComp.cpp
	Purpose: contains the main function of the project

	@author Warley Almeida Silva
	@version 1.0
*/

#include "stdafx.h"
#include <ilcplex/ilocplex.h>
#include <windows.h>
#include <vector>
#include <time.h>
#define HI +99999 // positive infinite
#define LI -99999 // negative infinite
#define MODEL1 1
#define MODEL2 2
#define FIXED_SOLUTION_MODEL 5

#define NOT_ACTIVE 0
#define ACTIVE 1
#define DONE 2
#define TO_BE_ACTIVE 3

using namespace std;

/*
	Dominance related variables
*/
int execCounter = 0;
bool flag = false;
vector<int> nodesToKeep;
int domNode = 2;

int previousNColumns = -1;
int previousModel = -1;
int previousCoefType = -1;
double previousInfCut = -1;

IloEnv env;
Funct ut;
LARGE_INTEGER t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, freq;
bool *ignored = NULL;
bool comparison = true;
stringstream ssIgnored;
vector<int> columnsIDs;

ostream& operator<<(ostream& os, const vector<int>& elements){
	os << "{";
	for (int i = 0; i < elements.size(); i++){
		os << elements[i];
		if(i != elements.size()-1) 
			os << "|";
	}
	os << "}";
	return os;
}

/*
	Checks if a node is ignored in a specific run of the algorithm
	*@param int node: node to be checked
	*@return bool: true if a node is ignored, false if it is not ignored
*********************************************************/
bool isIgnored(int node){
	return ignored[node - 1];
}

/*
	Frees the ignored array and set its value to NULL
	*@param -
	*@return void: -
*********************************************************/
void freeIgnored(){
	delete[] ignored;
	ignored = NULL;
}

/*
	Defines the coefficient type based on a model number
	*@param int model:	model number
	*@return int:		coefficient type
*********************************************************/
int defineCoefType(int model){
	if (model == 1 || model == 4)
		return 0;
	else
		return 1;
}

/*
	Checks whether the model is the same from previous run
	*@param int coefType:	model number
			double infCut:	information cut parameter
			int nColumns:	generated number of columns
	*@return bool:			true if new columns do not need to be generated, false if they do
*********************************************************/
bool checkCoefType(int coefType, double infCut, int nColumns){
	bool answer = false;
	if (coefType == previousCoefType && infCut == previousInfCut && nColumns == previousNColumns)
		answer = true;
	previousCoefType = coefType;
	previousInfCut = infCut;
	previousNColumns = nColumns;
	return answer;
}

/*
	Calculates the amount of time between two diferent time marks
	*@param LARGE_INTEGER t1: indicates the first time mark
			LARGE_INTEGER t2: indicates the second time mark
			LARGE_INTEGER f: indicates the frequency of the processor
	*@return int: the amount of time between t2 and t1
*********************************************************/
int time(LARGE_INTEGER *t2, LARGE_INTEGER *t1, LARGE_INTEGER *f){
	return (int)(((t2->QuadPart - t1->QuadPart) * 1000) / f->QuadPart);
}

/*
	Generates the column of a specific node 
	*@param Graph *graph:				is a pointer to the graph
			bool *generatedColumns:		generatedColumns[u] indicates whether a column for node u was generated or not
			int node:					stores the node that will generate the new column
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			IloRangeArray R:			is an array of constraints following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloNumVarArray W:			is an array with variables W following the model
			double cut:					determines when the information diffusion tree will be cut
			int model:					indicates which model is used in this run
			bool option:				indicates whether the percentage amount will be shown
	*@return void: -
*********************************************************/
void createColumn(Graph *graph, bool *generatedColumns, int node, Dictionary *allowedNodes, IloRangeArray R, IloNumVarArray Z, IloNumVarArray W, double cut, int model, bool option = false){
	Tree tree;
	graph->breadthSearch(&tree, node, cut);
	register int index = allowedNodes->getIndexByNode(node);
	if (generatedColumns[index]){
		cout << "Coluna: " << node << " | Index: " << index << endl;
		ut.wait("Erro ao gerar colunas");
	}
	if (ignored != NULL && isIgnored(node) == comparison){
		cout << "Node " << node << " ignored due to " << (comparison ? "high" : "low") << " similarity" << endl;
		return;
	}
	generatedColumns[index] = true;
	double control = 0;
	int current;
	register int dIndex;
	for (int f = 0; f < tree.getSize(); f++){
		current = tree.nodes[f];
		dIndex = allowedNodes->getIndexByNode(current);
		if (option)
			ut.load(f, tree.getSize(), &control);
		if ((model == 2 || model == 3 || model == 5) && tree.info[f] > 0){
			R[dIndex].setLinearCoef(Z[index], -1);
		}
		if ((model == 0) && tree.info[f] > 0){
			R[dIndex].setLinearCoef(Z[index], -1);
			if (node == domNode)
				R[dIndex].setLinearCoef(Z[index], IloInfinity);
		}
		if ((model == 1 || model == 4) && tree.getInfoByVertex(current) != 0){
			R[dIndex].setLinearCoef(Z[index], (double)-1 * tree.info[f]);
		}
	}
	R[index].setLinearCoef(Z[index], -1);
	if (model == 0 && node == domNode){
		R[index].setLinearCoef(Z[index], IloInfinity);
	}
}

/*
	Populates the arrays R, Z and W based on the network's information
	*@param Graph *graph:				is a pointer to the graph
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			IloRangeArray R:			is an array of constraints following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloNumVarArray W:			is an array with variables W following the model
	*@return void: -
*********************************************************/
void build(Graph *graph, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, IloRangeArray R, IloRangeArray T){
	double control = 0;
	stringstream auxText;
	Funct utilities;
	register int index;
	cout << " > Building:      ";
	for (int u = 0; u < graph->getNumberOfNodes(); u++){
		if (graph->isConnected(u + 1)){
			index = allowedNodes->add(u + 1);
			auxText << "w(" << u + 1 << ")";
			W.add(IloNumVar(env, 0, 1, auxText.str().c_str()));
			auxText.str("");
			auxText << "z(" << u + 1 << ")";
			Z.add(IloBoolVar(env, 0, 1, auxText.str().c_str()));
			auxText.str("");
			auxText << "r_" << u + 1;
			R.add(IloRange(env, -IloInfinity, W[index], 0, auxText.str().c_str()));
			auxText.str("");
			ut.load(u, graph->getNumberOfNodes(), &control);
		}
	}
	cout << endl;
}

/*
	Generates the columns of the run based on some parameters
	*@param Graph *graph:				is a pointer to the graph
			bool *generatedColumns:		generatedColumns[u] indicates whether a column for node u was generated or not
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			IloRangeArray R:			is an array with variables R following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloNumVarArray W:			is an array with variables W following the model
			double cut:					determines when the information diffusion tree will be cut
			int amount:					indicates the number of columns that will be generated
			int orderingBy:				indicates the criteria of sorting
			int model:					indicates which model is used in this run
	*@return void: -
*********************************************************/
void columns(Graph *graph, bool *generatedColumns, Dictionary *allowedNodes, IloRangeArray R, IloNumVarArray Z, IloNumVarArray W, double cut, int amount, int orderingBy, int model){
	vector<int> allowedZ;
	graph->getInitialNodes(&allowedZ, allowedNodes, orderingBy, amount);	
	int coefType = defineCoefType(model);
	if (!checkCoefType(coefType, cut, amount)){
		ut.setBool(generatedColumns, allowedNodes->getSize(), false);
		columnsIDs.clear();
		for (int i = 0; i < R.getSize(); i++)
			R[i].setExpr(W[i]);

		cout << " > Columns:      ";
		double control = 0;
		int counter = 0;
		for (int z : allowedZ){
			createColumn(graph, generatedColumns, z, allowedNodes, R, Z, W, cut, model, false);
			columnsIDs.push_back(z);
			counter += 1;
			ut.load(counter, (int)allowedZ.size(), &control);
		}
		cout << endl;
	}else{
		cout << " > Columns: Previous model with same coefficient type, no need for new columns" << endl;		
	}
}

/*
	Sets the CPLEX parameters
	*@param IloCplex cplex:				indicates the current CPLEX solver
			int initialC:				indicates the number of initial columns
			int initialR:				indicates the number of constraints
			int timelimit:				indicates in minutes the time limit
			int model:					indicates which model is used in this run
	*@return void: -
*********************************************************/
void parameters(IloCplex cplex, int initialC, int initialR, int timelimit, int model){
	cplex.setParam(IloCplex::PolishAfterDetTime, 0);

	cplex.setParam(IloCplex::TiLim, timelimit * 60);
	cplex.setParam(IloCplex::MemoryEmphasis, true);
	cplex.setParam(IloCplex::WorkMem, 100);
	cplex.setParam(IloCplex::NodeFileInd, 3);
	cplex.setParam(IloCplex::ColReadLim, initialC);
	cplex.setParam(IloCplex::RowReadLim, initialR);
	cplex.setParam(IloCplex::NumericalEmphasis, 1); 

	cplex.setParam(IloCplex::RINSHeur, 30);
	cplex.setParam(IloCplex::LBHeur, 1);
	cplex.setParam(IloCplex::MIPEmphasis, CPX_MIPEMPHASIS_HIDDENFEAS);

	if (flag)
		cplex.setParam(IloCplex::FPHeur, 2);
	cplex.setParam(IloCplex::AdvInd, 1);
	cout << "AdvInd: " << cplex.getParam(IloCplex::AdvInd) << endl;
}

void makeSolutionConstraint(IloRange previous, IloNumVarArray Z, IloNumArray solution){
	cout << " > Adding T constraints based on solution" << endl;
	int counter = 0;
	if (solution.getSize() != 0){
		for (int i = 0; i < solution.getSize(); i++){
			if (solution[i] == 1){
				previous.setLinearCoef(Z[i], 1);
				counter++;
			}
		}
	}
	previous.setUB(counter);
	previous.setLB(counter);
	cout << " > Solution passed by parameter size: " << solution.getSize() << endl;
}

/*
	Generates the constraints and objective function for model 001.

	In this model, Z is a binary variable and W is a real variable.
	It aims to maximize the amount of received information by members of the network (sum of W).
	It is constrained by the number of people that receives the initial information -> only maxInf nodes.
	Portuguese: Z bin�rio, W fracion�rio, maximiza quantidade de informa��o recebida (somat�rio de W) atendendo no m�ximo maxInf pessoas

	*@param IloModel model:				indicates the current CPLEX model
			float parameter:			indicates the upper bound in the objective function
			IloObjective objective:		indicates the current objective function
			IloRange information:		indicates the information constraint
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
	*@return void: -
	*********************************************************/
void model001(IloModel model, float parameter, IloObjective objective, IloRange information, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, bool * generatedColumns, IloNumArray solution, IloRange previous){
	IloConversion(env, W, IloNumVar::Float);
	information.setExpr(IloSum(Z));
	information.setUb(parameter);
	objective.setExpr(IloSum(W));
	objective.setSense(IloObjective::Maximize);
}

/*
	Generates the constraints and objective function for model 002.

	In this model, Z is a binary variable and W is a binary variable.
	It aims to maximize the amount of reached members of the network (sum of W).
	It is constrained by the number of people that receives the initial information -> only maxInf nodes.
	Portuguese: Z bin�rio, W bin�rio, maximiza n�mero de pessoas alcan�adas (somat�rio de W) atendendo no m�ximo maxInf pessoas

	*@param IloModel model:				indicates the current CPLEX model
			float parameter:			indicates the upper bound in the objective function
			IloObjective objective:		indicates the current objective function
			IloRange information:		indicates the information constraint
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
	*@return void: -
*********************************************************/
void model002(IloModel model, float parameter, IloObjective objective, IloRange information, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, bool *generatedColumns, IloNumArray solution, IloRange previous) {
	IloConversion(env, W, IloNumVar::Bool);
	IloConversion(env, Z, IloNumVar::Float);
	information.setUb(parameter);
	information.setExpr(IloSum(Z));
	objective.setExpr(IloSum(W));
	objective.setSense(IloObjective::Maximize);
}

/*
	Generates the constraints and objective function for model 003.

	In this model, Z is a binary variable and W is a binary variable.
	It aims to minimize the amount of chosen members of the network to receive the initial information (sum of Z).
	It is constrained by attending at least a percentage of the network indicated by parameter.
	Portuguese: Z bin�rio, W bin�rio, minimiza n�mero de pessoas escolhidas inicialmente (somat�rio de Z) atendendo no m�nimo uma porcentagem da rede

	*@param IloModel model:				indicates the current CPLEX model
			float parameter:			indicates the percentage of people in the network that must be reached
			IloObjective objective:		indicates the current objective function
			IloRange information:		indicates the information constraint
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
	*@return void: -
*********************************************************/
void model003(IloModel model, float parameter, IloObjective objective, IloRange information, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, IloNumArray solution, IloRange previous){
	IloConversion(env, W, IloNumVar::Bool);
	objective.setExpr(IloSum(Z));
	objective.setSense(IloObjective::Minimize);
	information.setExpr(IloSum(W));
	information.setLb(allowedNodes->getSize()*parameter);
}

/*
	Generates the constraints and objective function for model 004.

	In this model, a previous solution is passed by as parameter and the value of the objective function is calculated.
	Portuguese: Recebe solu��o inicial e verifica qual � o valor da funcao objetivo.

	*@param IloModel model:				indicates the current CPLEX model
			float parameter:			indicates the upper bound in the objective function
			IloObjective objective:		indicates the current objective function
			IloRange information:		indicates the information constraint
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloNumArray solution:		previous solution received as parameter
			IloRangeArray T:			constraint array that sets values to variables
	*@return void: -
*********************************************************/
void model004(IloModel model, float parameter, IloObjective objective, IloRange information, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, IloNumArray solution, IloRange previous){
	IloConversion(env, W, IloNumVar::Float);
	objective.setExpr(IloSum(W));
	objective.setSense(IloObjective::Maximize);
	information.setExpr(-IloSum(Z));
	information.setLb(-IloInfinity);
	makeSolutionConstraint(previous, Z, solution);
}

/*
	Generates the constraints and objective function for model 004.

	In this model, a previous solution is passed by as parameter and the value of the objective function is calculated.
	Portuguese: Recebe solu��o inicial e verifica qual � o valor da funcao objetivo.

	*@param IloModel model:				indicates the current CPLEX model
			float parameter:			indicates the upper bound in the objective function
			IloObjective objective:		indicates the current objective function
			IloRange information:		indicates the information constraint
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloNumArray solution:		previous solution received as parameter
			IloRangeArray T:			constraint array that sets values to variables
	*@return void: -
*********************************************************/
void model005(IloModel model, float parameter, IloObjective objective, IloRange information, Dictionary *allowedNodes, IloNumVarArray W, IloNumVarArray Z, bool *generatedColumns, IloNumArray solution, IloRange previous){
	IloConversion(env, W, IloNumVar::Bool);
	objective.setExpr(IloSum(W));
	objective.setSense(IloObjective::Maximize);
	information.setUb(parameter);
	information.setExpr(IloSum(Z));
	makeSolutionConstraint(previous, Z, solution);
}

string criteriaToString(int criteria){
	switch (criteria){
	case 0: return "SIW"; break;
	case 1: return "Degree"; break;
	case 2: return "Relative degree"; break;
	case 3: return "Closeness"; break;
	case 4: return "Relative closeness"; break;
	case 5: return "Eccentricity"; break;
	case 6: return "Radial"; break;
	}
	return "Not found";
}

/*
	Solves the previously constructed model
	*@param int modelNumber:			indicates the model that is being solved
			IloModel model:				indicates the current CPLEX model
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloRangeArray R:			is an array of constraints following the model
			IloNumArray solution:		stores a previous solution
			Graph *graph:				is a pointer to the graph
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			float parameter:			stores the information to set the upper bound of the information constraint
			float infCut:				determines when the information diffusion tree will be cut
			int initialC:				indicates the number of initial columns
			int timelimit:				indicates in minutes the time limit
			string append:				stores the path to the file to be written
			bool *generatedColumns:		generatedColumns[u] indicates whether a column for node u was generated or not
			int criteria:				indicates the criteria of sorting
	*@return void: -
*********************************************************/
int solve(int modelNumber, IloModel model, IloNumVarArray W, IloNumVarArray Z, IloRangeArray R, IloNumArray solution, Graph *graph, Dictionary *allowedNodes, float parameter, float infCut, int initialC, int timelimit, string append, bool *generatedColumns, int criteria, bool branching = false){
	
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&t0);
	int nReached = 0;
	ofstream output(append.c_str(), ios::app);

	stringstream auxText;
	IloRange information(env, 0, IloInfinity, "inf");
	IloRange previous(env, 0, 0, "prev");
	IloObjective objective(env);
	IloRangeArray T(env);
	
	/*
		LB variables
	*/
	IloRange localB(env, 0, IloInfinity, "localB");
	int lbRadius = (int) parameter;
	int lbPreviousSolution = LI;
	int lbMaxNumIteration = 1;
	int lbCounterWOImprov = 0;
	int lbCounter = 0;
	
	switch (modelNumber){
		case 1: model001(model, parameter, objective, information, allowedNodes, W, Z, generatedColumns, solution, previous); break;
		case 2: model002(model, parameter, objective, information, allowedNodes, W, Z, generatedColumns, solution, previous); break;
		case 3: model003(model, parameter, objective, information, allowedNodes, W, Z, solution, previous); break;
		case 4: model004(model, parameter, objective, information, allowedNodes, W, Z, solution, previous); break;
		case 5: model005(model, parameter, objective, information, allowedNodes, W, Z, generatedColumns, solution, previous); break;
		default: cout << "Wrong model parameter" << endl;
	}

	model.add(information);
	model.add(objective);
	model.add(previous);
	if (branching)
		model.add(localB);

	previousModel = modelNumber;

	IloCplex cplex(env);
	cplex.extract(model);
	
	do{
		/*
			LB start
		*/
		if (branching){
			localB.setLinearCoefs(Z, solution);
			lbRadius = lbRadius - 2;
			if (lbRadius < 0)
				lbRadius = 0;
			localB.setLB(lbRadius);
			lbCounter++;
			lbPreviousSolution = nReached;
			cout << " # Watch out, we are branching! Previous solution: " << lbPreviousSolution << ", Iteration: " << lbCounter << ", Radius:" << lbRadius << endl;
		}

		cout << " > Setting CPLEX parameters ... ";
		parameters(cplex, initialC, allowedNodes->getSize(), timelimit, modelNumber);
		cout << "finished setting parameters!" << endl << endl;

		if (solution.getSize() != 0){
			try{
				cplex.addMIPStart(Z, solution, IloCplex::MIPStartAuto, "previousSolution");
			}
			catch (IloException e){
				cerr << e.getMessage() << endl;
			}
		}

		if (!cplex.solve()) {
			env.error() << " > Failed to optimize LP. Please go over the log file to find out what happened." << endl;
		}
		else{
			stringstream a;
			a << "lastModel" << execCounter++ << ".lp";
			cplex.exportModel(a.str().c_str());

			vector<int> solutionIDs, reachedIDs;
			int sTreeSize = 0, minTreeSize = HI, maxTreeSize = LI;
			float sInversedWeight = 0, minInversedWeight = HI, maxInversedWeight = LI;
			solution.clear();

			IloNum solutionValue = cplex.getObjValue();

			nReached = 0;
			env.out() << endl << "**************************************************************************" << endl;
			env.out() << " | MODEL 00" << modelNumber << endl;
			switch (modelNumber){
			case 1: env.out() << " | maxInf: " << parameter; break;
			case 2: env.out() << " | maxInf: " << parameter; break;
			case 3: env.out() << " | Percentage: " << parameter * 100 << "%"; break;
			case 4: env.out() << " | maxInf: " << parameter; break;
			case 5: env.out() << " | maxInf: " << parameter; break;
			}
			env.out() << "\tSolution status = " << cplex.getStatus() << "\tSolution value = " << solutionValue << endl;
			env.out() << " | infCut = " << infCut << "\tOrdering by: " << criteriaToString(criteria) << endl;
			env.out() << " > People who received original information:" << endl;
			for (int i = 0; i < allowedNodes->getSize(); i++){
				int node = allowedNodes->getNodeByIndex(i);
				if (generatedColumns[i] && cplex.getValue(Z[i]) == 1){
					solutionIDs.push_back(node);
					solution.add(1);
					Tree tree;
					graph->breadthSearch(&tree, node, infCut);
					env.out() << " | ID:  " << node << "\tSIW: " << graph->getSIW(node) << "\tTree size:  " << tree.getSize() << "\tIndex: " << i << endl;
					sInversedWeight += graph->getSIW(node);
					sTreeSize += tree.getSize();
					if (tree.getSize() < minTreeSize)
						minTreeSize = tree.getSize();
					if (tree.getSize() > maxTreeSize)
						maxTreeSize = tree.getSize();
					if (graph->getSIW(node) < minInversedWeight)
						minInversedWeight = graph->getSIW(node);
					if (graph->getSIW(node) > maxInversedWeight)
						maxInversedWeight = graph->getSIW(node);
				}
				else{
					solution.add(0);
				}
				if (cplex.getValue(W[i]) >= infCut){
					reachedIDs.push_back(node);
					nReached += graph->getReachedNodes(allowedNodes->getNodeByIndex(i));
				}
			}
			switch (modelNumber){
			case 1: printf(" | Reached without merged nodes %d\t\t\t\tPercentage: %.2f%%\n", nReached, ((float)nReached / graph->getNumberOfNodes()) * 100); break;
			case 2: printf(" | Reached without merged nodes %d\t\t\t\tPercentage: %.2f%%\n", nReached, ((float)nReached / graph->getNumberOfNodes()) * 100); break;
			case 3: printf(" | Number of selected people: %.0f\t\t\t\tPercentage: %.2f%%\n", solutionValue, ((float)solutionValue / graph->getNumberOfNodes()) * 100); break;
			case 4: printf(" | Reached without merged nodes %d\t\t\t\tPercentage: %.2f%%\n", nReached, ((float)nReached / graph->getNumberOfNodes()) * 100); break;
			case 5: printf(" | Reached without merged nodes %d\t\t\t\tPercentage: %.2f%%\n", nReached, ((float)nReached / graph->getNumberOfNodes()) * 100); break;
			default: cout << "Wrong model parameter" << endl;
			}
			printf(" | Reached counting merged nodes: %d\t\t\t\tPercentage: %.2f%%\n", nReached, ((float)nReached / graph->getNumberOfNodes()) * 100);
			env.out() << "**************************************************************************" << endl;

			QueryPerformanceCounter(&t1);

			//output << "model, parameter, nColumns, infCut, time, solutionValue, meanInversedWeight, minInversedWeight, maxInversedWeight, meanTreeSize, minTreeSize, maxTreeSize, pReached, nReached, solutionIDs, comparison, orderingBy, columnIDs, reachedIDs, ignored" << endl;

			output << modelNumber << "," << parameter << "," << initialC << "," << infCut << "," << time(&t1, &t0, &freq) << "," << solutionValue << ",";
			output << (solutionIDs.size() != 0 ? (float)(sInversedWeight / solutionIDs.size()) : -1) << "," << minInversedWeight << "," << maxInversedWeight << ",";
			output << (solutionIDs.size() != 0 ? (float)(sTreeSize / solutionIDs.size()) : -1) << "," << minTreeSize << "," << maxTreeSize << ",";
			output << ((float)nReached / graph->getNumberOfNodes()) * 100 << "," << nReached << ",";
			output << solutionIDs << "," << (comparison ? "high" : "low") << "," << criteriaToString(criteria) << "," << columnsIDs << ",";
			output << reachedIDs << "," <<ssIgnored.str() << endl;

		}

		/*
			LB end
		*/
		if (branching && nReached <= lbPreviousSolution)
			lbCounterWOImprov++;
			

	} while (branching && lbCounterWOImprov < lbMaxNumIteration && lbCounter < parameter && lbRadius > 0);
	cplex.end();
	information.end();
	objective.end();
	previous.end();

	cout << " > Finishing function solve" << endl;
	return nReached;
}

/*
	Call the build and solve functions previously implemented
	*@param int modelNumber:			indicates the model that is being solved
			IloModel model:				indicates the current CPLEX model
			IloNumVarArray W:			is an array with variables W following the model
			IloNUmVarArray Z:			is an array with variables Z following the model
			IloRangeArray R:			is an array of constraints following the model
			IloNumArray solution:		stores a previous solution
			Graph *graph:				is a pointer to the graph
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			bool *generatedColumns:		generatedColumns[u] indicates whether a column for node u was generated or not
			float parameter:			stores the information to set the upper bound of the information constraint
			float infCut:				determines when the information diffusion tree will be cut
			int initialC:				indicates the number of initial columns
			int timelimit:				indicates in minutes the time limit
			string path:				stores the path to the file to be written
			int criteria:				indicates the criteria of sorting
	*@return void: -
*********************************************************/
int run(int modelNumber, IloModel model, IloNumVarArray W, IloNumVarArray Z, IloRangeArray R, IloNumArray solution, Graph *graph, Dictionary *allowedNodes, bool * generatedColumns, float parameter, float infCut, int initialC, int timelimit, string path, int criteria, bool branching = false){
	cout << endl << " $ Running model " << modelNumber << " with parameters: " << endl;
	cout << " | infCut: " << infCut << endl;
	cout << " | maxInf : " << parameter << endl;
	cout << " | nColumns : " << initialC << endl;
	cout << " | Ordering by: " << criteriaToString(criteria) << endl;
	columns(graph, generatedColumns, allowedNodes, R, Z, W, infCut, initialC, criteria, modelNumber);
	return solve(modelNumber, model, W, Z, R, solution, graph, allowedNodes, parameter, infCut, initialC, timelimit, path, generatedColumns, criteria, branching);
}

/*
	Greedy approach to solve model 3
	*@param Graph *graph:				is a pointer to the graph
			float percentage:			indicates the percentage of people in the network that must be reached
	*@return void: -
*********************************************************/
void greedy(Graph * graph, float percentage){
	int counter = 0;
	int current = 0;
	int size = graph->getNumberOfNodes();
	heapNode * candidates;
	candidates = new heapNode[size];
	bool *chosen = new bool[size];
	for (int i = 0; i < size; i++){
		candidates[i].id = i + 1;
		candidates[i].chave = (float) graph->getDegree(i + 1);
		chosen[i] = false;
	}
	ut.heapSort(candidates, size);
	while (counter < percentage*graph->getNumberOfNodes()){
		current = 0;
		Tree tree;
		graph->breadthSearch(&tree, candidates[0].id, 0.001);
		cout << candidates[0].id << " | ";
		current++;
		candidates[0].chave = -1;
		for (int i = 0; i < tree.getSize(); i++){
			if (!chosen[tree.nodes[i] - 1]){
				chosen[tree.nodes[i] - 1] = true;
				current++;
			}
		}
		for (int i = 0; i < graph->getNumberOfNodes(); i++){
			if (chosen[candidates[i].id - 1])
				candidates[i].chave = -1;
		}
		ut.heapSort(candidates, size);
		size -= current;
		counter += current;
	}
}

/*
	Creates the ignored array based on specific criterias
	*@param Graph *graph:				is a pointer to the graph
			Dictionary *allowedNodes:	dictionary with all allowed nodes
			int c1, c2:					indicate the criterias that will be considered in this run
			int nColumns:				indicates the number of columns that will be generated
	*@return void: -
*********************************************************/
void createIgnored(Graph *graph, Dictionary *allowedNodes, int c1, int c2, int nColumns){
	if (ignored != NULL) freeIgnored();
	ignored = new bool[graph->getNumberOfNodes()];
	ssIgnored.str("");
	ssIgnored << "WO considering columns in " << c1 << " and " << c2 << " at the same time";
	vector<int> listc1, listc2;
	graph->getInitialNodes(&listc1, allowedNodes, c1, nColumns);
	graph->getInitialNodes(&listc2, allowedNodes, c2, nColumns);
	bool *bc1 = new bool[graph->getNumberOfNodes()];
	bool *bc2 = new bool[graph->getNumberOfNodes()];
	for (int i = 0; i < graph->getNumberOfNodes(); i++){
		bc1[i] = false;
		bc2[i] = false;
	}
	for(int j = 0; j < listc1.size(); j++){
		bc1[listc1[j] - 1] = true;
		bc2[listc2[j] - 1] = true;
	}
	for (int i = 0; i < graph->getNumberOfNodes(); i++){
		ignored[i] = bc1[i] && bc2[i];
	}
}

int linearThreshold(Graph *g, vector<int> chosen_set, float infCut){

	int *current_state = new int[g->getNumberOfNodes()];
	float *sumOfWeights = new float[g->getNumberOfNodes()];
	float *toBeAdded = new float[g->getNumberOfNodes()];

	bool flag = true;
	vector<int> adj;

	for (int i = 0; i < g->getNumberOfNodes(); i++){
		current_state[i] = NOT_ACTIVE;
		sumOfWeights[i] = 0;
		toBeAdded[i] = 0;
	}


	for (int node : chosen_set){
		current_state[node - 1] = ACTIVE;
		g->getAdjacency(&adj, node);
		for (int anode : adj){
			sumOfWeights[anode - 1] += g->getWeight(node, anode);
		}
		adj.clear();
	}
	
	int active_counter = 0, loop = 0;

	while (flag){
		flag = false;
		for (int i = 0; i < g->getNumberOfNodes(); i++){
			if (sumOfWeights[i] >= infCut && current_state[i] == NOT_ACTIVE){
				//cout << "Exploring " << i + 1 << "..." << endl;
				current_state[i] = TO_BE_ACTIVE;
				g->getAdjacency(&adj, i + 1);
				for (int node : adj)
					toBeAdded[node - 1] += g->getWeight(node, i + 1);
				adj.clear();
			}
		}

		active_counter = 0;
		loop++;
		for (int i = 0; i < g->getNumberOfNodes(); i++){
			if (current_state[i] == TO_BE_ACTIVE){
				current_state[i] = ACTIVE;
				flag = true;
				active_counter++;
			}
			sumOfWeights[i] += toBeAdded[i];
			toBeAdded[i] = 0;
		}
		cout << "Active at iteration " << loop << ": " << active_counter << endl;
	}

	int count = 0;
	for (int i = 0; i < g->getNumberOfNodes(); i++)
		if (current_state[i] == ACTIVE)
			count++;

	cout << count << "\n";

	return count;
}

int independentCascade(Graph *g, vector<int> chosen_set, float infCut){

	int *current_state = new int[g->getNumberOfNodes()];

	bool flag = true;
	vector<int> adj;

	for (int i = 0; i < g->getNumberOfNodes(); i++)
		current_state[i] = NOT_ACTIVE;

	for (int node : chosen_set){
		current_state[node - 1] = ACTIVE;
	}

	int active_counter = 0, loop = 0;

	while (flag){
		flag = false;
		for (int i = 0; i < g->getNumberOfNodes(); i++){
			if (current_state[i] == ACTIVE){
				//cout << "Exploring " << i + 1 << "..." << endl;
				current_state[i] = DONE;
				g->getAdjacency(&adj, i+1);
				for (int node : adj)
					if (g->getWeight(node, i + 1) >= infCut && current_state[node - 1] == NOT_ACTIVE)
						current_state[node - 1] = TO_BE_ACTIVE;
					
				
				adj.clear();
			}
		}				

		active_counter = 0;
		loop++;
		for (int i = 0; i < g->getNumberOfNodes(); i++){
			if (current_state[i] == TO_BE_ACTIVE){
				current_state[i] = ACTIVE;
				flag = true;
				active_counter++;
			}
		}		
		cout << "Active at iteration " << loop <<  ": " << active_counter << endl;
	}

	int count = 0;
	for (int i = 0; i < g->getNumberOfNodes(); i++)
		if (current_state[i] == DONE)
			count++;

	cout << count << "\n";

	return count;
}

//void greedyApproach(Graph *g, float infCut, int k, char type = 'c'){
//	int count = 0;
//	int sum = 0;
//	int highestSum = 0;
//	int highestSumNode = -1;
//	list<int> set;
//	while (count < k){
//		cout << "Looking for node with highest sum... " << endl;
//		for (int i = 0; i < g->getNumberOfNodes(); i++){
//			switch (type) {
//				case 't': sum = linearThreshold(g, set, i + 1, infCut); break;
//				case 'c': sum = independentCascade(g, set, i + 1, infCut); break;
//				default: exit(0);
//			}
//			
//			if (sum > highestSum){
//				highestSum = sum;
//				highestSumNode = i + 1;
//			}
//		}
//		cout << highestSumNode << " found" << endl;
//		set.push_back(highestSumNode);
//	}
//	cout << "Set: ";
//	while (!set.empty()){
//		cout << set.front() << " | ";
//		set.pop_front();
//	}
//}

void createSolution(Graph *graph, Dictionary *allowedNodes, IloNumArray solution, int criteria, int amount){
	vector<int> nodes;
	ofstream output("createdByCriteria.txt", ios::app);
	cout << " > Selecting " << amount << " better nodes based on criteria " << criteriaToString(criteria) << " ... ";
	output << criteriaToString(criteria) << ",";
	solution.clear();
	graph->getInitialNodes(&nodes, allowedNodes, criteria, amount);
	for (int i = 0; i < allowedNodes->getSize(); i++)
		solution.add(0);
	for (int node : nodes){
		solution[allowedNodes->getIndexByNode(node)] = 1;
		output << node << ";";
	}
	output << endl;
	cout << "finished!" << endl;
}

void createSolutionFromList(int *nodes, int numberOfNodes, IloNumArray solution, Dictionary *allowedNodes){
	for (int i = 0; i < allowedNodes->getSize(); i++){
		solution.add(0);
	}
	for (int i = 0; i < numberOfNodes; i++){
		solution[allowedNodes->getIndexByNode(nodes[i])] = 1;
	}
}

void createListFromSolution(vector<int> *list, IloNumArray solution, Dictionary *allowedNodes){
	for (int i = 0; i < solution.getSize(); i++)
		if (solution[i] == 1)
			list->push_back(allowedNodes->getNodeByIndex(i));	
}

int dominance(int node, IloModel model, IloNumVarArray W, IloNumVarArray Z, IloRangeArray R, Graph *graph, Dictionary *allowedNodes, bool * generatedColumns, float infCut, int initialC, int criteria){
	domNode = node;	
	int counter = 0;
	int total = 0;
	cout << endl << " $ Running dominance check for z" << node << ": " << endl;
	columns(graph, generatedColumns, allowedNodes, R, Z, W, infCut, initialC, criteria, 0);

	IloConversion conversion(env, Z, IloNumVar::Float);
	model.add(conversion);
	IloObjective objective(env);
	objective.setSense(IloObjective::Maximize);
	objective.setExpr(IloSum(Z));
	model.add(objective);
	for (int i = 0; i < allowedNodes->getSize(); i++){
		R[i].setLinearCoef(W[i], 0);
		R[i].setLb(0);
		R[i].setUb(IloInfinity);
	}
	try{
		IloCplex cplex(model);
		cplex.exportModel("domModel.lp");

		if (cplex.solve()){
			cout << "Sucesso!" << endl;
			env.out() << "\tSolution status = " << cplex.getStatus() << "\tSolution value = " << cplex.getObjValue() << endl;
			IloNumArray valuesZ(env);
			cplex.getValues(Z, valuesZ);
			for (int i = 0; i < valuesZ.getSize(); i++){
				if (generatedColumns[i])
					total += 1;
				if (generatedColumns[i] && valuesZ[i] == 1){
					cout << "z" << node << " domina z" << allowedNodes->getNodeByIndex(i) << endl;
					counter += 1;
				}
				//if (generatedColumns[i] && valuesZ[i] == 0)
					//cout << "z" << node << " nao domina z" << allowedNodes->getNodeByIndex(i) << endl;
			}
			//cout << model << endl;
			cout << "Node " << node << " dominates " << counter << " other columns, which represents " << (float)counter / (float)total << endl;
		}
		else{
			cout << "Falha!" << endl;
		}
		cplex.end();
		objective.end();
	}
	catch (IloException err){
		cout << err.getMessage() << endl;
	}
	model.remove(conversion);

	if (previousCoefType == -1)
		previousCoefType = 0;
	else
		previousCoefType = !previousCoefType;
	
	return counter;
}

int main(int argc, char **argv){

	Graph graph;
	graph.load("lorenza.txt");
	Dictionary allowedNodes(graph.getNumberOfNodes());
	
	IloModel model(env);
	IloNumVarArray W(env);
	IloNumVarArray Z(env);
	IloRangeArray R(env);
	IloRangeArray T(env);
	IloNumArray solution(env);

	build(&graph, &allowedNodes, W, Z, R, T);

	model.add(R);
	
	bool *generatedColumns = new bool[allowedNodes.getSize()];

	int tlimit = 4*60;
	int ncolumns = 10000;//allowedNodes.getSize();
	string file = "results.csv";
	int value = 0;
	float infCuts[] = { (float) 0.001, (float) 0.005, (float) 0.01, (float) 0.1 };
	int criterion[] = { SIW, DEGREE, CLOSENESS, RADIAL };
	
	ofstream output("comparsion_LT_IC.csv", ios::app);
	//output << "model,maxinf,infCut,approach,amountReached,percentageReached" << endl;
	
	/*
		base test!
	
	value = run(1, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, maxInf, infCut, ncolumns, tlimit, file, SIW);
	value = run(2, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, maxInf, infCut, ncolumns, tlimit, file, SIW);
	output << maxInf << "," << infCut << ",MIP," << value << "," << (float)value / (float)graph.getNumberOfNodes() << endl;
	solution.clear();
	for (int i = 0; i < 3; i++){
		createSolution(&graph, &allowedNodes, solution, criterion[i], (int) maxInf);
		value = run(5, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, maxInf, infCut, ncolumns, tlimit, file, SIW);
		output << maxInf << "," << infCut << "," << criteriaToString(criterion[i]) << "," << value << "," << (float)value / (float)graph.getNumberOfNodes() << endl;
	}*/

	/*
		Calculates the solution for the heuristics approaches
	*/

	/*for (float infCut = 0.1; infCut < 1; infCut += 0.1){
		for (int maxInf = 10; maxInf < 100; maxInf += 10){
			for (int w = 0; w < 4; w++){
				createSolution(&graph, &allowedNodes, solution, criterion[w], (int) maxInf);
				value = run(FIXED_SOLUTION_MODEL, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, (float)maxInf, infCut, ncolumns, tlimit, file, SIW, false);
				output << FIXED_SOLUTION_MODEL << "," << maxInf << "," << infCut << "," << criteriaToString(criterion[w]) << "," << value << "," << (float)value / (float)graph.getNumberOfNodes() << endl;
			}
		}
	}*/

	/*
		Calculates the solution for our approach 
	*/

	/*for (float infCut = 0.1; infCut < 1; infCut += 0.1){
		for (int maxInf = 10; maxInf < 200; maxInf += 10){
			value = run(MODEL1, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, (float) maxInf, infCut, ncolumns, tlimit, file, SIW);
			value = run(MODEL2, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, (float) maxInf, infCut, ncolumns, tlimit, file, SIW);
			output << "1|2," << maxInf << "," << infCut << ",MIP," << value << "," << (float)value / (float)graph.getNumberOfNodes() << endl;
		}
	}*/

	vector<int> ids; 
	vector<int> ids_model;
	int result = 0;

	for (int maxInf = 10; maxInf <= 100; maxInf += 10){

		value = run(MODEL1, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, maxInf, 0.01, ncolumns, tlimit, file, SIW);
		value = run(MODEL2, model, W, Z, R, solution, &graph, &allowedNodes, generatedColumns, maxInf, 0.01, ncolumns, tlimit, file, SIW);
		createListFromSolution(&ids_model, solution, &allowedNodes);

		for (float i = 0.01; i < 1.0; i += 0.01){			

			result = linearThreshold(&graph, ids_model, i);
			output << "Our model;LT;" << i << ";" << result << endl;

			for (int j = 0; j < 4; j++){
				cout << criteriaToString(criterion[j]) << endl;
				graph.getInitialNodes(&ids, &allowedNodes, criterion[j], maxInf);
				result = linearThreshold(&graph, ids, i);
				ids.clear();
				output << criteriaToString(criterion[j]) << ";LT;" << i << ";" << result << ";" << maxInf << endl;
			}
		}

		for (float i = 0.01; i < 1.0; i += 0.01){

			
			result = independentCascade(&graph, ids_model, i);
			output << "Our model;IC;" << i << ";" << result << endl;

			for (int j = 0; j < 4; j++){
				cout << criteriaToString(criterion[j]) << endl;
				graph.getInitialNodes(&ids, &allowedNodes, criterion[j], maxInf);
				result = independentCascade(&graph, ids, i);
				ids.clear();
				output << criteriaToString(criterion[j]) << ";LT;" << i << ";" << result << ";" << maxInf << endl;
			}
		}

		ids_model.clear();

	}

	
	
	ut.wait("aff");
}
