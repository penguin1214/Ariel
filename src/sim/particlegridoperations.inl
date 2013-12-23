// Ariel: FLIP Fluid Simulator
// Written by Yining Karl Li
//
// File: particlegridsplat.inl
// Breakout file for some particle-grid operations stuff

#ifndef PARTICLEGRIDOPERATIONS_INL
#define PARTICLEGRIDOPERATIONS_INL

#include "../grid/macgrid.inl"
#include "../grid/particlegrid.hpp"
#include "../grid/levelset.hpp"
#include "../utilities/utilities.h"
#include "../grid/gridutils.inl"
#include <omp.h>

using namespace std;
using namespace glm;

namespace fluidCore {
//====================================
// Struct and Function Declarations
//====================================

//Forward declarations for externed inlineable methods
extern inline void splatParticlesToMACGrid(particlegrid* sgrid, vector<particle*>& particles,
										   macgrid& mgrid);
extern inline void splatMACGridToParticles(vector<particle*>& particles, macgrid& mgrid);
extern inline void enforceBoundaryVelocity(macgrid& mgrid);
extern inline vec3 interpolateVelocity(vec3 p, macgrid& mgrid);
inline float checkWall(intgrid* A, const int& x, const int& y, const int& z);
inline float interpolate(floatgrid* q, vec3 p, vec3 n);
	
//====================================
// Function Implementations
//====================================

float checkWall(intgrid* A, const int& x, const int& y, const int& z){
	if(A->getCell(x,y,z)==SOLID){ //inside wall
		return 1.0f;
	}else{
		return -1.0f;
	}
}

void enforceBoundaryVelocity(macgrid& mgrid){
	int x = (int)mgrid.dimensions.x;
	int y = (int)mgrid.dimensions.y;
	int z = (int)mgrid.dimensions.z;

	//for every x face
	#pragma omp parallel for
	for(int i = 0; i < x+1; i++){  
	  	for(int j = 0; j < y; j++){ 
	    	for(int k = 0; k < z; k++){
	    		if(i==0 || i==x){
	    			mgrid.u_x->setCell(i,j,k, 0.0f);
	    		}
				if( i<x && i>0 && checkWall(mgrid.A, i, j, k)*checkWall(mgrid.A, i-1, j, k) < 0 ) {
					mgrid.u_x->setCell(i,j,k, 0.0f);
				}
	    	}
	   	}
	} 
	//for every y face
	#pragma omp parallel for
	for(int i = 0; i < x; i++){  
	  	for(int j = 0; j < y+1; j++){ 
	    	for(int k = 0; k < z; k++){
	    		if(j==0 || j==y){
	    			mgrid.u_y->setCell(i,j,k, 0.0f);
	    		}
				if( j<y && j>0 && checkWall(mgrid.A, i, j, k)*checkWall(mgrid.A, i, j-1, k) < 0 ) {
					mgrid.u_y->setCell(i,j,k, 0.0f);
				}
	    	}
	   	}
	} 
	//for every z face
	#pragma omp parallel for
	for(int i = 0; i < x; i++){  
	  	for(int j = 0; j < y; j++){ 
	    	for(int k = 0; k < z+1; k++){
	    		if(k==0 || k==z){
	    			mgrid.u_z->setCell(i,j,k, 0.0f);
	    		}
				if( k<z && k>0 && checkWall(mgrid.A, i, j, k)*checkWall(mgrid.A, i, j, k-1) < 0 ) {
					mgrid.u_z->setCell(i,j,k, 0.0f);
				}
	    	}
	   	}
	} 
}

float interpolate(floatgrid* q, vec3 p, vec3 n){
	float x = glm::max(0.0f,glm::min(n.x,p.x));
	float y = glm::max(0.0f,glm::min(n.y,p.y));
	float z = glm::max(0.0f,glm::min(n.z,p.z));
	int i = glm::min(x,n.x-2);
	int j = glm::min(y,n.y-2);
	int k = glm::min(z,n.z-2);
	float term1 = ((i+1-x)*q->getCell(i,j,k)+(x-i)*q->getCell(i+1,j,k))*(j+1-y);
	float term2 = ((i+1-x)*q->getCell(i,j+1,k)+(x-i)*q->getCell(i+1,j+1,k))*(y-j);
	float term3 = ((i+1-x)*q->getCell(i,j,k+1)+(x-i)*q->getCell(i+1,j,k+1))*(j+1-y);
	float term4 = ((i+1-x)*q->getCell(i,j+1,k+1)+(x-i)*q->getCell(i+1,j+1,k+1))*(y-j);
	return (k+1-z)*(term1 + term2) + (z-k)*(term3 + term4);
}

vec3 interpolateVelocity(vec3 p, macgrid& mgrid){
	int x = (int)mgrid.dimensions.x; int y = (int)mgrid.dimensions.y; int z = (int)mgrid.dimensions.z;
	vec3 u;
	u.x = interpolate(mgrid.u_x, vec3(x*p.x, y*p.y-0.5f, z*p.z-0.5f), vec3(x+1, y, z));
	u.y = interpolate(mgrid.u_y, vec3(x*p.x-0.5f, y*p.y, z*p.z-0.5f), vec3(x, y+1, z));
	u.z = interpolate(mgrid.u_z, vec3(x*p.x-0.5f, y*p.y-0.5f, z*p.z), vec3(x, y, z+1));
	return u;
}

void splatMACGridToParticles(vector<particle*>& particles, macgrid& mgrid){
	int particleCount = particles.size();
	#pragma omp parallel for
	for(int i=0; i<particleCount; i++){
		particles[i]->u = interpolateVelocity(particles[i]->p, mgrid);
	}
}

void splatParticlesToMACGrid(particlegrid* sgrid, vector<particle*>& particles, macgrid& mgrid){
	
	float RE = 1.4f; //sharpen kernel weight

	int x = (int)mgrid.dimensions.x; int y = (int)mgrid.dimensions.y; int z = (int)mgrid.dimensions.z;
	
	#pragma omp parallel for
	for(int i = 0; i < x+1; i++){
		for(int j = 0; j < y+1; j++){
			for(int k = 0; k < z+1; k++){
				vector<particle*> neighbors;
				//Splat X direction
				if(j<y && k<z){
					vec3 px = vec3(i, j+0.5f, k+0.5f);
					float sumw = 0.0f;
					float sumx = 0.0f;
					neighbors = sgrid->getWallNeighbors(vec3(i,j,k), vec3(1,2,2));
					for(int n=0; n<neighbors.size(); n++){
						particle* p = neighbors[n];
						if(p->type == FLUID){
							vec3 pos;
							pos.x = fmax(0,fmin(x,x*p->p.x));
							pos.y = fmax(0,fmin(y,y*p->p.y));
							pos.z = fmax(0,fmin(z,z*p->p.z));
							float w = p->mass * mathCore::sharpen(mathCore::sqrlength(pos,px),RE);
							sumx += w*p->u.x;
							sumw += w;
						}
					}
					float uxsum = 0.0f;
					if(sumw>0){ 
						uxsum = sumx/sumw;
					}
					mgrid.u_x->setCell(i,j,k,uxsum);
				}
				neighbors.clear();

				//Splat Y direction
				if(i<x && k<z){
					vec3 py = vec3(i+0.5f, j, k+0.5f);
					float sumw = 0.0f;
					float sumy = 0.0f;
					neighbors = sgrid->getWallNeighbors(vec3(i,j,k), vec3(2,1,2));
					for(int n=0; n<neighbors.size(); n++){
						particle* p = neighbors[n];
						if(p->type == FLUID){
							vec3 pos;
							pos.x = fmax(0,fmin(x,x*p->p.x));
							pos.y = fmax(0,fmin(y,y*p->p.y));
							pos.z = fmax(0,fmin(z,z*p->p.z));
							float w = p->mass * mathCore::sharpen(mathCore::sqrlength(pos,py),RE);
							sumy += w*p->u.y;
							sumw += w;
						}
					}
					float uysum = 0.0f;
					if(sumw>0){
						uysum = sumy/sumw;
					}
					mgrid.u_y->setCell(i,j,k,uysum);
				}
				neighbors.clear();

				//Splat Z direction
				if(i<x && j<y){
					vec3 pz = vec3(i+0.5f, j+0.5f, k);
					float sumw = 0.0f;
					float sumz = 0.0f;
					neighbors = sgrid->getWallNeighbors(vec3(i,j,k), vec3(2,2,1));
					for(int n=0; n<neighbors.size(); n++){
						particle* p = neighbors[n];
						if(p->type == FLUID){
							vec3 pos;
							pos.x = fmax(0,fmin(x,x*p->p.x));
							pos.y = fmax(0,fmin(y,y*p->p.y));
							pos.z = fmax(0,fmin(z,z*p->p.z));
							float w = p->mass * mathCore::sharpen(mathCore::sqrlength(pos,pz),RE);
							sumz += w*p->u.z;
							sumw += w;
						}
					}
					float uzsum = 0.0f;
					if(sumw>0){
						uzsum = sumz/sumw;
					}
					mgrid.u_z->setCell(i,j,k,uzsum);
				}
				neighbors.clear();
			}
		}
	}
}

}

#endif
