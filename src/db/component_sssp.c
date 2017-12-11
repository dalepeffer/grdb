#include <fcntl.h>
#include <math.h>
#include <limits.h>
#include "graph.h"
#include "cli.h"	


/* Place the code for your Dijkstra implementation in this file */

//Find the number of vertices in the graph
int vCount(){
	off_t off;
	ssize_t len;
	char s[BUFSIZE], *buf;
	int fd, length, verticesCount;
	length = sizeof(vertex_t);
	verticesCount = 0;
	buf = malloc(length);

	//Open vertex file/get file descriptor
	memset(s, 0, BUFSIZE);
	sprintf(s, "%s/%d/%d/v", grdbdir, gno, cno);
	fd = open(s, O_RDONLY);

	//Read in vertices from the file until no vertices remain 
	for (off = 0;;off += length){
		lseek(fd, off, SEEK_SET);
		len = read(fd, buf, length);
		if (len <= 0)
			break;
		verticesCount += 1;
	}

	//Close file
	close(fd);

	return verticesCount;
}


int
component_sssp(
        component_t c,
        vertexid_t v1,
        vertexid_t v2,
        int *n,
        int *total_weight,
        vertexid_t **path)
{

	//Get the weight of edge (u, v) out of the database
	int getEdgeWeight(vertexid_t u, vertexid_t v) {
		char s[BUFSIZE];
		int edgeFile, edgeWeight, offset;
		struct edge _edge;
		attribute_t attribute;

		edgeWeight = -1;

		//open edge file
		memset(s, 0, BUFSIZE);
		sprintf(s, "%s/%d/%d/e", grdbdir, gno, cno);
		edgeFile = open(s, O_RDONLY);

		//get edge
		edge_init(&_edge); //create empty edge
		edge_set_vertices(&_edge, u, v); //assign vertices to edge
		edge_read(&_edge, c->se, edgeFile); //read edge from db matching edge just created

		//read edge weight
		if (_edge.tuple != NULL){ //only continue if edge was found in database
			attribute = _edge.tuple->s->attrlist;
			offset = tuple_get_offset(_edge.tuple, attribute->name);
			if (offset >= 0){ 
				edgeWeight = tuple_get_int(_edge.tuple->buf + offset);
			}
		}

		close(edgeFile);

		return(edgeWeight);
	}

	void init_GC(int verticesCount, int G[verticesCount][verticesCount], int C[verticesCount][verticesCount]){
		int result;
		for (int i = 0; i < verticesCount; i += 1){
			for (int j = 0; j < verticesCount; j += 1){
				result = getEdgeWeight(i + 1, j + 1);
				if (result >= 0){
					G[i][j] = 1;
					C[i][j] = result;
				} else {
					G[i][j] = 0;
					C[i][j] = INT_MAX;
				}
			}
		}
	}

	void init_SD(int verticesCount, int S[verticesCount], int D[verticesCount], vertexid_t P[verticesCount], vertexid_t v1){
		for (int i = 0; i < verticesCount; i += 1){
			if (i == (v1)){
				S[i] = 1;
				D[i] = 0;
			} else {
				S[i] = 0;
				D[i] = INT_MAX;
			}
			P[i] = -1;
		}
	}

	void buildPath(vertexid_t **path, vertexid_t new, int *path_size){ 
	//Add vertex to path array
		vertexid_t *newPathArray = realloc(*path, (*path_size + 1) * sizeof(vertexid_t));
		if (newPathArray){
			newPathArray[*path_size] = new + 1;
			*path = newPathArray;
			*path_size += 1;
		}
	}

	void SSPGetPath(int verticesCount, vertexid_t P[verticesCount]){
	//Go backwards from v2 to the source vertex to get the shortest path

		vertexid_t current, temp[verticesCount];
		int m = verticesCount - 1;
		current = v2;

		//Initialize temp
		for (int i = 0; i < verticesCount; i += 1){
			temp[i] = -1;
		}

		while (P[current] != -1){
			temp[m] = current;
			current = P[current];
			m -= 1;
		}

		for (int i = m + 1; i < verticesCount; i += 1){
			buildPath(path, temp[i], n);
		}
	}

	//Checks if all vertices in the graph have been added to S
	int is_full(int verticesCount, int S[verticesCount]){
		int isFull = 1;
		for (int i = 0; i < verticesCount; i += 1){
			if (S[i] == 0)
				isFull = 0;
		}

		return isFull;
	}

	//Check if a vertex v is in S
	int in_S(int verticesCount, int S[verticesCount], vertexid_t v){
		return S[v];
	}

	//Get the cheapest edge connected to S that's not already in S
	vertexid_t shortestPath(int verticesCount, 
							   int S[verticesCount], 
							   int G[verticesCount][verticesCount], 
							   int C[verticesCount][verticesCount])
	{
		int minimumWeight = INT_MAX;
		vertexid_t newVertex, parentVertex;

		for (vertexid_t i = 0; i < verticesCount; i += 1){
			if (S[i] == 1){   //If vertex i in S
				//Check costs of going to adjacent vertices not in S 
				for (vertexid_t j = 0; j < verticesCount; j += 1){
					if (!in_S(verticesCount, S, j) && C[i][j] < minimumWeight){
						minimumWeight = C[i][j];
						parentVertex = i;
						newVertex = j;
					}
				}
			}
		}

		S[newVertex] = 1;

		return newVertex;
	}

	void relaxGraphEdges(int verticesCount, vertexid_t w, 
					 int G[verticesCount][verticesCount], 
					 int C[verticesCount][verticesCount], 
					 int D[verticesCount], 
					 vertexid_t P[verticesCount])
	{
		for (int i = 0; i < verticesCount; i += 1){
			if (G[w][i] && D[i] > (D[w] + C[w][i])){
				D[i] = D[w] + C[w][i];
				P[i] = w;
			}
		}

	
	}

	/*Execute Dijkstra on the attribute you found for the specifiedcomponent*/
	v1 -= 1;
	v2 -= 1;
	int verticesCount = vCount();
	vertexid_t w, P[verticesCount];        //P = parent list
	int S[verticesCount], D[verticesCount]; //S = min path found list, D = Current distance list
	int G[verticesCount][verticesCount], C[verticesCount][verticesCount]; //G = adjacency matrix, C = cost matrix
	vertexid_t *new_path = malloc(sizeof(vertexid_t));


	//Initialize path
	if (new_path){
		*path = new_path;
		*path[0] = v1 + 1;
	}
	*n = 1;

	//Initialize matrices and lists
	init_SD(verticesCount, S, D, P, v1);
	init_GC(verticesCount, G, C);

	//Relax edges connected to v1
	relaxGraphEdges(verticesCount, v1, G, C, D, P);

	//Add cheapest edges and relax until all shortest paths found
	while(!is_full(verticesCount, S)){
		w = shortestPath(verticesCount, S, G, C);
		relaxGraphEdges(verticesCount, w, G, C, D, P);
	}

	//Print out weight of minimum paths to each vertex
	for (int i = 0; i < verticesCount; i += 1){
		printf("Min cost/distance to vertex %i is %i\n", i + 1, D[i]);
	}

	SSPGetPath(verticesCount, P);
	*total_weight = D[v2];	

	return 0;
}
