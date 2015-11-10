#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ddd.h"

static bool recursiveTraverseGraph(Vertex* root, Vertex* v, int marker);

void initGraph(Graph* graph)
{
    memset(graph->hashtable, 0, sizeof(graph->hashtable));
    graph->freeEdges = NULL;
    graph->freeVertexes = NULL;
    graph->marker = 0;
}

static inline Edge* newEdge(Graph* graph)
{
    Edge* edge = graph->freeEdges;
    if (edge == NULL) { 
        edge = (Edge*)malloc(sizeof(Edge));
    } else { 
        graph->freeEdges = edge->next;
    }
    return edge;
}

static inline void freeVertex(Graph* graph, Vertex* vertex)
{
    int h = vertex->xid % MAX_TRANSACTIONS;
    Vertex** vpp = &graph->hashtable[h];
    while (*vpp != vertex) { 
        vpp = &(*vpp)->next;
    }
    *vpp = vertex->next;
    vertex->next = graph->freeVertexes;
    graph->freeVertexes = vertex;

}

static inline void freeEdge(Graph* graph, Edge* edge)
{
    edge->next = graph->freeEdges;
    graph->freeEdges = edge;
}

static inline Vertex* newVertex(Graph* graph)
{
    Vertex* v = graph->freeVertexes;
    if (v == NULL) { 
        v = (Vertex*)malloc(sizeof(Vertex));
    } else { 
        graph->freeVertexes = v->next;
    }
    return v;
}

static inline Vertex* findVertex(Graph* graph, xid_t xid)
{
    xid_t h = xid % MAX_TRANSACTIONS;
    Vertex* v;
    for (v = graph->hashtable[h]; v != NULL; v = v->next) { 
        if (v->xid == xid) { 
            return v;
        }
    }
    v = newVertex(graph);
    l2_list_init(&v->outgoingEdges);
    v->xid = xid;
    v->nIncomingEdges = 0;
    v->next = graph->hashtable[h];
    graph->hashtable[h] = v;
    return v;
}

void addSubgraph(Instance* instance, Graph* graph, xid_t* xids, int n_xids)
{
    xid_t *last = xids + n_xids;
    Edge *e, *next, *edges = NULL;
    while (xids != last) { 
        Vertex* src = findVertex(graph, *xids++);
        xid_t xid;
        while ((xid = *xids++) != 0) { 
            Vertex* dst = findVertex(graph, xid);
            e = newEdge(graph);
            dst->nIncomingEdges += 1;
            e->dst = dst;
            e->src = src;
            e->next = edges;
            edges = e;
            l2_list_link(&src->outgoingEdges, &e->node);
        }
    }
    for (e = instance->edges; e != NULL; e = next) { 
        next = e->next;
        l2_list_unlink(&e->node);
        if (--e->dst->nIncomingEdges == 0 && l2_list_is_empty(&e->dst->outgoingEdges)) {
            freeVertex(graph, e->dst);
        }
        if (e->src->nIncomingEdges == 0 && l2_list_is_empty(&e->src->outgoingEdges)) {
            freeVertex(graph, e->src);
        }
        freeEdge(graph, e);
    }
}

static bool recursiveTraverseGraph(Vertex* root, Vertex* v, int marker)
{
    L2List* l;
    Edge* e;
    v->visited = marker;
    for (l = v->outgoingEdges.next; l != &v->outgoingEdges; l = e->node.next) {
        e = (Edge*)l;
        if (e->dst == root) { 
            return true;
        } else if (e->dst->visited != marker && recursiveTraverseGraph(root, e->dst, marker)) { /* loop */
            return true;
        } 
    }
    return false;        
}

bool findLoop(Graph* graph, xid_t root)
{
    Vertex* v;
    for (v = graph->hashtable[root % MAX_TRANSACTIONS]; v != NULL; v = v->next) { 
        if (v->xid == root) { 
            return recursiveTraverseGraph(v, v, ++graph->marker);
        }
    }
    return false;        
}
