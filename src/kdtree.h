#ifndef LIBKDTREE_LIBRARY_H
#define LIBKDTREE_LIBRARY_H

// _MSC_VER
// Defined as an integer literal that encodes the major and
// minor number elements of the compiler's version number. 
// __declspec
// which specifies that an instance of a given type is to be stored with
// a Microsoft-specific storage-class attribute listed below.
// When you declare a class dllexport, all its member functions and
// static data members are exported. 
#if defined(_MSC_VER)
#define DLLExport  __declspec(dllexport)
#else
#define DLLExport
#endif


#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>

struct DLLExport tree_node
{
    size_t id;
    size_t split;
    tree_node *left, *right;
};

struct DLLExport tree_model
{
    tree_node *root;
    const float *datas;
    const float *labels;
    size_t n_samples;
    size_t n_features;
    float p;
};


DLLExport void free_tree_memory(tree_node *root);
DLLExport tree_model* build_kdtree(const float *datas, const float *labels,
                                   size_t rows, size_t cols, float p);
DLLExport float* k_nearests_neighbor(const tree_model *model, const float *X_test,
                                     size_t len, size_t k, bool clf);
DLLExport void find_k_nearests(const tree_model *model, const float *coor,
                              size_t k, size_t *args, float *dists);


#ifdef __cplusplus
}
#endif


#endif