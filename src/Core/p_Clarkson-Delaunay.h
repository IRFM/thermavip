/* Clarkson-Delaunay.h */
/*
 * Ken Clarkson wrote this.  Copyright (c) 1995 by AT&T..
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 */
#ifndef CLARKSON_DELAUNAY_H
#define CLARKSON_DELAUNAY_H


//static int sees(site, simplex*);
//static void buildhull(simplex*);
//static simplex* facets_print(simplex* s, void* p);
//static simplex* visit_triang_gen(simplex* s, visit_func* visit, test_func* test);

int* BuildTriangleIndexList(void* pointList, float factor, int numberOfInputPoints, int numDimensions, int clockwise, int* numTriangleVertices);

#endif
