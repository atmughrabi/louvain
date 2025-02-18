#include "partition.h"

#define NLINKS2 8

void printClusterPartition(louvainPartition *partition)
{

    unsigned long  v;

    printf(" -----------------------------------------------------\n");
    printf("| %-21s | %-27s | \n", "size", "neighCommNb");
    printf(" -----------------------------------------------------\n");
    printf("| %-21lu | %-27lu | \n",  partition->size, partition->neighCommNb);
    printf(" ----------------------------------------------------------------------------------------------------------\n");
    printf("| %-15s | %-15s | %-15s | %-15s | %-15s | %-15s | \n", "V", "node2Community", "in", "tot", "neighCommWeights", "neighCommPos");
    printf(" ----------------------------------------------------------------------------------------------------------\n");


    for (v = 0; v < partition->size; v++)
    {
        printf("| %-15lu | %-15lu | %-15Lf | %-15Lf | %-15Lf | %-15lu | \n", v, partition->node2Community[v], partition->in[v], partition->tot[v], partition->neighCommWeights[v], partition->neighCommPos[v]);

    }
    printf(" ----------------------------------------------------------------------------------------------------------\n");
}

int myCompare (const void *a, const void *b, void *array2)
{
    long diff = ((unsigned long *)array2)[*(unsigned long *)a] - ((unsigned long *)array2)[*(unsigned *)b];
    int res = (0 < diff) - (diff < 0);
    return  res;
}

unsigned long *mySort(unsigned long *part, unsigned long size)
{
    unsigned long i;
    unsigned long *nodes = (unsigned long *)malloc(size * sizeof(unsigned long));
    for (i = 0; i < size; i++)
    {
        nodes[i] = i;
    }
    qsort_r(nodes, size, sizeof(unsigned long), myCompare, (void *)part);
    return nodes;
}



inline long double degreeWeighted(adjlist *g, unsigned long node)
{
    unsigned long long i;
    if (g->weights == NULL)
    {
        return 1.*(g->cd[node + 1] - g->cd[node]);
    }

    long double res = 0.0L;
    for (i = g->cd[node]; i < g->cd[node + 1]; i++)
    {
        res += g->weights[i];
        // printf("v:%lu u:%llu w:%Lf tot:%Lf \n", node, i, g->weights[i], res);
    }

    // printf("v:%lu tot:%Lf \n\n", node, res);
    return res;
}

inline long double selfloopWeighted(adjlist *g, unsigned long node)
{
    unsigned long long i;

    for (i = g->cd[node]; i < g->cd[node + 1]; i++)
    {
        if (g->adj[i] == node)
        {
            return (g->weights == NULL) ? 1.0 : g->weights[i];
        }
    }

    return 0.0;
}

inline void removeNode(louvainPartition *p, adjlist *g, unsigned long node, unsigned long comm, long double dnodecomm)
{
    p->in[comm]  -= 2.0L * dnodecomm + selfloopWeighted(g, node);
    p->tot[comm] -= degreeWeighted(g, node);
}

inline void insertNode(louvainPartition *p, adjlist *g, unsigned long node, unsigned long comm, long double dnodecomm)
{
    p->in[comm]  += 2.0L * dnodecomm + selfloopWeighted(g, node);
    p->tot[comm] += degreeWeighted(g, node);

    p->node2Community[node] = comm;
}

inline long double gain(louvainPartition *p, adjlist *g, unsigned long comm, long double dnc, long double degc)
{
    long double totc = p->tot[comm];
    long double m2   = g->totalWeight;

    return (dnc - totc * degc / m2);
}

//freeing memory
void free_adjlist2(adjlist *g)
{
    free(g->cd);
    free(g->adj);
    free(g->weights);
    free(g);
}

void freeLouvainPartition(louvainPartition *p)
{
    free(p->in);
    free(p->tot);
    free(p->neighCommWeights);
    free(p->neighCommPos);
    free(p->node2Community);
    free(p);
}


louvainPartition *createLouvainPartition(adjlist *g)
{
    unsigned long i;

    louvainPartition *p = malloc(sizeof(louvainPartition));

    p->size = g->n;

    p->node2Community = malloc(p->size * sizeof(unsigned long));
    p->in = malloc(p->size * sizeof(long double));
    p->tot = malloc(p->size * sizeof(long double));

    p->neighCommWeights = malloc(p->size * sizeof(long double));
    p->neighCommPos = malloc(p->size * sizeof(unsigned long));
    p->neighCommNb = 0;

    for (i = 0; i < p->size; i++)
    {
        p->node2Community[i] = i;
        p->in[i]  = selfloopWeighted(g, i);
        p->tot[i] = degreeWeighted(g, i);
        p->neighCommWeights[i] = -1;
        p->neighCommPos[i] = 0;
    }

    return p;
}

long double modularity(louvainPartition *p, adjlist *g)
{
    long double q  = 0.0L;
    long double m2 = g->totalWeight;
    unsigned long i;

    for (i = 0; i < p->size; i++)
    {
        if (p->tot[i] > 0.0L)
            q += p->in[i] - (p->tot[i] * p->tot[i]) / m2;
    }

    return q / m2;
}

void neighCommunitiesInit(louvainPartition *p)
{
    unsigned long i;
    for (i = 0; i < p->neighCommNb; i++)
    {
        p->neighCommWeights[p->neighCommPos[i]] = -1;
    }
    p->neighCommNb = 0;
}

/*
Computes the set of neighbor communities of a given node (excluding self-loops)
*/
void neighCommunities(louvainPartition *p, adjlist *g, unsigned long node)
{
    unsigned long long i;
    unsigned long neigh, neighComm;
    long double neighW;
    p->neighCommPos[0] = p->node2Community[node];
    p->neighCommWeights[p->neighCommPos[0]] = 0.;
    p->neighCommNb = 1;

    // for all neighbors of node, add weight to the corresponding community
    for (i = g->cd[node]; i < g->cd[node + 1]; i++)
    {
        neigh  = g->adj[i];
        neighComm = p->node2Community[neigh];
        neighW = (g->weights == NULL) ? 1.0 : g->weights[i];
        // printf("%lu %lu %Lf\n", neigh, neighComm, neighW);
        // if not a self-loop
        if (neigh != node)
        {
            // if community is new (weight == -1)
            if (p->neighCommWeights[neighComm] == -1)
            {
                p->neighCommPos[p->neighCommNb] = neighComm;
                p->neighCommWeights[neighComm] = 0.;
                p->neighCommNb++;
            }
            p->neighCommWeights[neighComm] += neighW;
        }
    }
}

/*
Same behavior as neighCommunities except:
- self loop are counted
- data structure if not reinitialised
*/
void neighCommunitiesAll(louvainPartition *p, adjlist *g, unsigned long node)
{
    unsigned long long i;
    unsigned long neigh, neighComm;
    long double neighW;

    for (i = g->cd[node]; i < g->cd[node + 1]; i++)
    {
        neigh  = g->adj[i];
        neighComm = p->node2Community[neigh];
        neighW = (g->weights == NULL) ? 1.0 : g->weights[i];

        // if community is new
        if (p->neighCommWeights[neighComm] == -1)
        {
            p->neighCommPos[p->neighCommNb] = neighComm;
            p->neighCommWeights[neighComm] = 0.;
            p->neighCommNb++;
        }
        p->neighCommWeights[neighComm] += neighW;
    }
}


unsigned long updatePartition(louvainPartition *p, unsigned long *part, unsigned long size)
{
    // Renumber the communities in p
    unsigned long *renumber = calloc(p->size, sizeof(unsigned long));
    unsigned long i, last = 1;
    for (i = 0; i < p->size; i++)
    {
        if (renumber[p->node2Community[i]] == 0)
        {
            renumber[p->node2Community[i]] = last++;
        }
    }

    // Update part with the renumbered communities in p
    for (i = 0; i < size; i++)
    {
        part[i] = renumber[p->node2Community[part[i]]] - 1;
    }

    free(renumber);
    return last - 1;
}


// Return the meta graph induced by a partition of a graph
// See Louvain article for more details
adjlist *louvainPartition2Graph(louvainPartition *p, adjlist *g)
{
    unsigned long node, i, j;
    // Renumber communities
    unsigned long *renumber = (unsigned long *)malloc(g->n * sizeof(unsigned long));
    for (node = 0; node < g->n; node++)
        renumber[node] = 0;
    unsigned long last = 1;
    for (node = 0; node < g->n; node++)
    {
        if (renumber[p->node2Community[node]] == 0)
        {
            renumber[p->node2Community[node]] = last++;
        }
    }
    for (node = 0; node < g->n; node++)
    {
        p->node2Community[node] = renumber[p->node2Community[node]] - 1 ;
    }

    // sort nodes according to their community
    unsigned long *order = mySort(p->node2Community, g->n);
    //  displayPartU(p->node2Community, g->n);

    // Initialize meta graph
    adjlist *res = (adjlist *)malloc(sizeof(adjlist));
    unsigned long long e1 = NLINKS2;
    res->n = last - 1;
    res->e = 0;
    res->cd = (unsigned long long *)calloc((1 + res->n), sizeof(unsigned long long));
    res->cd[0] = 0;
    res->adj = (unsigned long *)malloc(NLINKS2 * sizeof(unsigned long));
    res->totalWeight = 0.0L;
    res->weights = (long double *)malloc(NLINKS2 * sizeof(long double));

    // for each node (in community order), extract all edges to other communities and build the graph
    neighCommunitiesInit(p);
    unsigned long oldComm = p->node2Community[order[0]];//renumber[p->node2Community[order[0]]];

    printClusterPartition(p);

    unsigned long currentComm;
    for (i = 0; i <= p->size; i++)
    {

        // current node and current community with dummy values if out of bounds
        node = (i == p->size) ? 0 : order[i];
        currentComm = (i == p->size) ? currentComm + 1 : p->node2Community[order[i]];
        printf("%lu %lu %lu \n", node, currentComm, oldComm);
        // new community, write previous one
        if (oldComm != currentComm)
        {
            res->cd[oldComm + 1] = res->cd[oldComm] + p->neighCommNb;
            //      displayPartU(res->degrees, res->n + 1);

            // for all neighboring communities of current community
            for (j = 0; j < p->neighCommNb; j++)
            {
                unsigned long neighComm = p->neighCommPos[j];
                long double neighCommWeight = p->neighCommWeights[p->neighCommPos[j]];

                // add edge in res
                res->adj[res->e] = neighComm;
                res->weights[res->e] = neighCommWeight;
                res->totalWeight += neighCommWeight;
                (res->e)++;
                printf("node:%lu oldComm:%lu currentComm:%lu neighComm:%lu neighCommWeight:%Lf new_num:%llu \n", node, oldComm, currentComm, neighComm, neighCommWeight, res->e);
                // reallocate edges and weights if necessary
                if (res->e == e1)
                {
                    e1 *= 2;
                    res->adj = (unsigned long *)realloc(res->adj, e1 * sizeof(unsigned long));
                    res->weights = (long double *)realloc(res->weights, e1 * sizeof(long double));
                    if (res->adj == NULL || res->weights == NULL)
                    {
                        printf("error during memory allocation\n");
                        exit(0);
                    }
                }

            }

                 // display(res);

            if (i == p->size)
            {
                res->adj = (unsigned long *)realloc(res->adj, res->e * sizeof(unsigned long));
                res->weights = (long double *)realloc(res->weights, res->e * sizeof(long double));

                free(order);
                free(renumber);

                return res;
            }

            oldComm = currentComm;
            neighCommunitiesInit(p);
        }

        // add neighbors of node i
        neighCommunitiesAll(p, g, node);
        //    displayNeighs(p, node);
    }

    printf("bad exit\n");
    return res;
}

// Compute one pass of Louvain and returns the improvement
long double louvainOneLevel(louvainPartition *p, adjlist *g)
{
    unsigned long nbMoves;
    long double startModularity = modularity(p, g);
    long double newModularity = startModularity;
    long double curModularity;
    unsigned long i, j, node;
    unsigned long oldComm, newComm, bestComm;
    long double degreeW, bestCommW, bestGain, newGain;


    // generate a random order for nodes' movements
    unsigned long *randomOrder = (unsigned long *)malloc(p->size * sizeof(unsigned long));
    for (unsigned long i = 0; i < p->size; i++)
        randomOrder[i] = i;
    for (unsigned long i = 0; i < p->size - 1; i++)
    {
        unsigned long randPos = rand() % (p->size - i) + i;
        unsigned long  tmp = randomOrder[i];
        randomOrder[i] = randomOrder[randPos];
        randomOrder[randPos] = tmp;
    }

    // repeat while
    //   there are some nodes moving
    //   or there is an improvement of quality greater than a given epsilon
    do
    {
        curModularity = newModularity;
        nbMoves = 0;

        // for each node:
        //   remove the node from its community
        //   compute the gain for its insertion in all neighboring communities
        //   insert it in the best community with the highest gain
        for (i = 0; i < g->n; i++)
        {
            node = i;//randomOrder[nodeTmp];
            oldComm = p->node2Community[node];
            degreeW = degreeWeighted(g, node);

            // computation of all neighboring communities of current node
            neighCommunitiesInit(p);
            neighCommunities(p, g, node);
            // printClusterPartition(p);
            // remove node from its current community
            removeNode(p, g, node, oldComm, p->neighCommWeights[oldComm]);

            // compute the gain for all neighboring communities
            // default choice is the former community
            bestComm = oldComm;
            bestCommW  = 0.0L;
            bestGain = 0.0L;
            for (j = 0; j < p->neighCommNb; j++)
            {
                newComm = p->neighCommPos[j];
                newGain = gain(p, g, newComm, p->neighCommWeights[newComm], degreeW);

                if (newGain > bestGain)
                {
                    bestComm = newComm;
                    bestCommW = p->neighCommWeights[newComm];
                    bestGain = newGain;
                }
            }

            // insert node in the nearest community
            insertNode(p, g, node, bestComm, bestCommW);

            if (bestComm != oldComm)
            {
                nbMoves++;
            }
        }

        newModularity = modularity(p, g);

        //    printf("%Lf\n", newModularity);

    }
    while (nbMoves > 0 &&
            newModularity - curModularity > MIN_IMPROVEMENT);

    free(randomOrder);

    return newModularity - startModularity;
}

unsigned long louvain(adjlist *g, unsigned long *lab)
{
    unsigned long i, n;
    long double improvement;
    // Initialize partition with trivial communities
    for (i = 0; i < g->n; i++)
    {
        lab[i] = i;
    }

    louvainPartition *gp = createLouvainPartition(g);
    louvainOneLevel(gp, g);
    n = updatePartition(gp, lab, g->n);
    freeLouvainPartition(gp);

    return n;
}

unsigned long louvainComplete(adjlist *g, unsigned long *lab)
{
    adjlist *init = g, *g2;
    unsigned long n, i;
    unsigned long long j;
    unsigned long originalSize = g->n;
    long double improvement;
    // Initialize partition with trivial communities
    for (i = 0; i < g->n; i++)
    {
        lab[i] = i;
    }

    // Execution of Louvain method
    while(1)
    {

        // printf(" createLouvainPartition \n");
        louvainPartition *gp = createLouvainPartition(g);
        // printClusterPartition(gp);

        // printf(" louvainOneLevel \n");
        improvement = louvainOneLevel(gp, g);
        // printClusterPartition(gp);

        // printf(" updatePartition \n");
        n = updatePartition(gp, lab, originalSize);
        printClusterPartition(gp);

        if (improvement < MIN_IMPROVEMENT)
        {
            freeLouvainPartition(gp);
            break;
        }

        g2 = louvainPartition2Graph(gp, g);

        printf("improvement:%Lf - total_weight:%Lf \n", improvement, g2->totalWeight);

        // free all graphs except the original one
        if (g->n < originalSize)
        {
            free_adjlist2(g);
        }
        freeLouvainPartition(gp);
        g = g2;
    }

    if (g->n < originalSize)
    {
        free_adjlist2(g);
    }

    return n;
}

