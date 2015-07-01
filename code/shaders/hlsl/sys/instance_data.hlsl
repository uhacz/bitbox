#ifndef INSTANCE_DATA
#define INSTANCE_DATA

#define MAX_WORLD_MATRICES 16
shared cbuffer InstanceData : register(b1)
{
    matrix world_matrix[MAX_WORLD_MATRICES];
	matrix world_it_matrix[MAX_WORLD_MATRICES];
};



#endif