/**
	Information Diffusion in Complex Networks
	dictionary.cpp
	Purpose: implements the class Dictionary

	@author Warley Almeida Silva
	@version 1.0
*/

#include "stdafx.h"

Dictionary::Dictionary(int t){
	size = t;
	info = new int[t];
	indexes = new int[t];
	lastIndex = 0;
	for (int u = 0; u < size; u++){
		info[u] = -1;
		indexes[u] = -1;
	}
}

Dictionary::~Dictionary(){
	size = -1;
	lastIndex = -1;
	delete info;
	info = NULL;
}

int Dictionary::add(int n){
	info[lastIndex] = n;
	indexes[n - 1] = lastIndex;
	lastIndex++;
	return lastIndex - 1;
}

int Dictionary::getIndexByNode(int n){
	return indexes[n-1];
}

int Dictionary::getNodeByIndex(int i){
	return info[i];
}

int Dictionary::getSize(){
	return lastIndex;
}

void Dictionary::print(){
	cout << "[";
	for (int u = 0; u < lastIndex; u++){
		cout << info[u]; 
		if (u+1!=lastIndex)
			cout << ", ";
	}
	cout << "]" << endl;
}