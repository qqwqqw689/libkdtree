#include "kdtree.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include <tuple>
#include <unordered_map>
#include <stack>
#include <queue>
#include <cstring>
#include <cassert>
#include <cstdlib>

// Example:
//     int x = Malloc(int, 10);
//     int y = (int *)malloc(10 * sizeof(int));
#define Malloc(type, n) (type *)malloc((n)*sizeof(type))

// If you need to use Intel MKL to accelerate,
// you can cancel the next line comment.

//#define USE_INTEL_MKL


#ifdef USE_INTEL_MKL
#include <mkl.h>
#endif

// Clang does not support OpenMP.
#ifndef __clang__

#include <omp.h>

#endif

// Non-recursive algorithm for freeing the memory of a binary tree
DLLExport void free_tree_memory(tree_node *root) {
    std::stack<tree_node *> node_stack;
    tree_node *p;
    node_stack.push(root);
    while (!node_stack.empty()) {
        p = node_stack.top();
        node_stack.pop();
        if (p->left)
            node_stack.push(p->left);
        if (p->right)
            node_stack.push(p->right);
        free(p);
    }
}


class KDTree {
public:
	KDTree(){}

    KDTree(tree_node *root, const float *datas, size_t rows, size_t cols, float p);

    KDTree(const float *datas, const float *labels,
           size_t rows, size_t cols, float p, bool free_tree = true);

    ~KDTree();

    tree_node *GetRoot() { return root; }

    std::vector<std::tuple<size_t, float>> FindKNearests(const float *coor, size_t k);

    std::tuple<size_t, float> FindNearest(const float *coor, size_t k) { return FindKNearests(coor, k)[0]; }

    void CFindKNearests(const float *coor, size_t k, size_t *args, float *dists);


private:
    // The sample with the largest distance from point `coor`
    // is always at the top of the heap.
    struct neighbor_heap_cmp {
        bool operator()(const std::tuple<size_t, float> &i,
                        const std::tuple<size_t, float> &j) {
            return std::get<1>(i) < std::get<1>(j);
        }
    };

    typedef std::tuple<size_t, float> neighbor;
    typedef std::priority_queue<neighbor,
            std::vector<neighbor>, neighbor_heap_cmp> neighbor_heap;

    // The heap (large top heap) when searching for K-nearest neighbor, the top of the heap is always
    // the farthest point of the sample point in the k-nearest neighbor
    neighbor_heap k_neighbor_heap_; // the second argumant of the tuple is distance.
    // the exponent or power, dist(x, y) = pow((x^p + y^p), 1/p)
    float p;
    // Whether to release the memory of the tree during deconstruction
    bool free_tree_;
    // Root node
    tree_node *root;
    // training set
    const float *datas;
    // The number of samples for the training set
    size_t n_samples;
    // Dimensions of each sample
    size_t n_features;
    // Label of training set
    const float *labels;
    // Cache pool used when searching for the median
    std::tuple<size_t, float> *get_mid_buf_;
    // Cache pool when searching for K nearest neighbor, 
    // visited_buf[i] = True if point i has already been searched
    bool *visited_buf_;

#ifdef USE_INTEL_MKL
    // Cache when using Intel MKL library
    float *mkl_buf_;
#endif


    // Initialize cache
    void InitBuffer();

    // build tree
    tree_node *BuildTree(const std::vector<size_t> &points);

    // Find the median of a set of numbers(dim-th dimension for each point in points)
    std::tuple<size_t, float> MidElement(const std::vector<size_t> &points, size_t dim);

    // push into stack
    void HeapStackPush(std::stack<tree_node *> &paths, tree_node *node, const float *coor, size_t k);

    // Get the value of the dim-th feature for the sample-th data point in the training set.
    float GetDimVal(size_t sample, size_t dim) {
        return datas[sample * n_features + dim];
    }

    // Find the distance of point coor from the i th point in the training set
    float GetDist(size_t i, const float *coor);

    // Look for the best segmentation feature
    // To disperse the dots
    size_t FindSplitDim(const std::vector<size_t> &points);

};

// Find the K-nearest neighbor of a tree. The id of Ki and the distance of Ki from coor are saved in args and dists
DLLExport
void find_k_nearests(const tree_model *model, const float *coor,
                     size_t k, size_t *args, float *dists) {
    KDTree tree(model->root, model->datas, model->n_samples, model->n_features, model->p);
    std::vector<std::tuple<size_t, float>> k_nearest = tree.FindKNearests(coor, k);
    for (size_t i = 0; i < k; ++i) {
        args[i] = std::get<0>(k_nearest[i]);
        dists[i] = std::get<1>(k_nearest[i]);
    }
}

// Build a KD-Tree
DLLExport
tree_model *build_kdtree(const float *datas, const float *labels,
                         size_t rows, size_t cols, float p) {
    KDTree tree(datas, labels, rows, cols, p, false);
    tree_model *model = Malloc(tree_model, 1);
    model->datas = datas;
    model->labels = labels;
    model->n_features = cols;
    model->n_samples = rows;
    model->root = tree.GetRoot();
    model->p = p;
    return model;
}

// Find the average value for regression problems
float mean(const float *arr, size_t len) {
    float ans = 0.0;
    for (size_t i = 0; i < len; ++i)
        ans += arr[i];
    return ans / len;
}

// Voting, used to classification problems
float vote(const float *arr, size_t len) {
    std::unordered_map<int, size_t> counter;
    for (size_t i = 0; i < len; ++i) {
        auto t = static_cast<int>(arr[i]);
        if (counter.find(t) == counter.end())
            counter.insert(std::unordered_map<int, size_t>::value_type(t, 1));
        else
            counter[t] += 1;
    }
    float cur_arg_max = 0;
    size_t cur_max = 0;
    for (auto &i : counter) {
        if (i.second >= cur_max) {
            cur_arg_max = static_cast<float>(i.first);
            cur_max = i.second;
        }
    }
    return cur_arg_max;
}

DLLExport float *
k_nearests_neighbor(const tree_model *model, const float *X_test, size_t len, size_t k, bool clf) {
	float *ans = Malloc(float, len);
	size_t *args;
	float *dists, *y_pred;
	size_t arr_len;
	int i, j;

#ifdef USE_INTEL_MKL
    int n_procs = omp_get_num_procs();
	assert(n_procs < 100);
	KDTree *trees[100];
	for (size_t i = 0; i < n_procs; ++i)
		trees[i] = new KDTree(model->root, model->datas, model->n_samples, model->n_features, model->p);
	arr_len = k * n_procs;
#else
	arr_len = k;
	KDTree tree(model->root, model->datas, model->n_samples, model->n_features, model->p);
#endif

	args = Malloc(size_t, arr_len);
	dists = Malloc(float, arr_len);
	y_pred = Malloc(float, arr_len);

#ifdef USE_INTEL_MKL
#pragma omp parallel for
	for (i = 0; i < len; ++i)
	{
		int thread_num = omp_get_thread_num();
		trees[thread_num]->CFindKNearests(X_test + i * model->n_features, 
			k, args + k * thread_num, dists + k * thread_num);
		for (j = 0; j < k; ++j)
			y_pred[j + k * thread_num] = model->labels[args[j + k * thread_num]];
		if (clf)
			ans[i] = vote(y_pred + k * thread_num, k);
		else
			ans[i] = mean(y_pred + k * thread_num, k);
	}
	for (size_t i = 0; i < n_procs; ++i)
		delete trees[i];
	
#else
	for (i = 0; i < len; ++i) {
		tree.CFindKNearests(X_test + i * model->n_features, k, args, dists);
		for (j = 0; j < k; ++j)
			y_pred[j] = model->labels[args[j]];
		if (clf)
			ans[i] = vote(y_pred, k);
		else
			ans[i] = mean(y_pred, k);
	}
#endif
	free(args);
	free(y_pred);
	free(dists);
	return ans;
}


inline KDTree::KDTree(tree_node *root, const float *datas, size_t rows, size_t cols, float p) :
        root(root), datas(datas), n_samples(rows),
        n_features(cols), p(p), free_tree_(false) {
    InitBuffer();
    labels = nullptr;
}

inline KDTree::KDTree(const float *datas, const float *labels, size_t rows, size_t cols, float p, bool free_tree) :
        datas(datas), labels(labels), n_samples(rows), n_features(cols), p(p), free_tree_(free_tree) {
    std::vector<size_t> points; // the index of points : [0,1,2,..,n_sample-1]
    for (size_t i = 0; i < n_samples; ++i)
        points.emplace_back(i);
    InitBuffer();
    root = BuildTree(points);
}

inline KDTree::~KDTree() {
    delete[] get_mid_buf_;
    delete[] visited_buf_;
#ifdef USE_INTEL_MKL
    free(mkl_buf_);
#endif
    if (free_tree_)
        free_tree_memory(root);
}

std::vector<std::tuple<size_t, float>> KDTree::FindKNearests(const float *coor, size_t k) {
    std::memset(visited_buf_, 0, sizeof(bool) * n_samples);
    std::stack<tree_node *> paths;
    tree_node *p = root;

    while (p) {
        HeapStackPush(paths, p, coor, k);
        p = coor[p->split] <= GetDimVal(p->id, p->split) ? p->left : p->right;
    }
    while (!paths.empty()) {
        p = paths.top();
        paths.pop();

        if (!p->left && !p->right)
            continue;

        if (k_neighbor_heap_.size() < k) {
            if (p->left)
                HeapStackPush(paths, p->left, coor, k);
            if (p->right)
                HeapStackPush(paths, p->right, coor, k);
        } else {
            float node_split_val = GetDimVal(p->id, p->split);
            float coor_split_val = coor[p->split];
            float heap_top_val = std::get<1>(k_neighbor_heap_.top());
            if (coor_split_val > node_split_val) {
                if (p->right)
                    HeapStackPush(paths, p->right, coor, k);

                if ((coor_split_val - node_split_val) < heap_top_val && p->left)
                    HeapStackPush(paths, p->left, coor, k); // ???
            } else {
                if (p->left)
                    HeapStackPush(paths, p->left, coor, k);
                if ((node_split_val - coor_split_val) < heap_top_val && p->right)
                    HeapStackPush(paths, p->right, coor, k); // ???
            }
        }
    }
    std::vector<std::tuple<size_t, float>> res;

    while (!k_neighbor_heap_.empty()) {
        res.emplace_back(k_neighbor_heap_.top());
        k_neighbor_heap_.pop();
    }
    return res;
}

void KDTree::CFindKNearests(const float *coor, size_t k, size_t *args, float *dists) {
    std::vector<std::tuple<size_t, float>> k_nearest = FindKNearests(coor, k);
    for (size_t i = 0; i < k; ++i) {
        args[i] = std::get<0>(k_nearest[i]);
        dists[i] = std::get<1>(k_nearest[i]);
    }
}


// Initialize cache

inline void KDTree::InitBuffer() {
    get_mid_buf_ = new std::tuple<size_t, float>[n_samples];
    visited_buf_ = new bool[n_samples];

#ifdef USE_INTEL_MKL
    // "To interact with C code, memory is allocated in the same way as C."
    mkl_buf_ = Malloc(float, n_features);
#endif
}

tree_node *KDTree::BuildTree(const std::vector<size_t> &points) {
    size_t dim = FindSplitDim(points);
    std::tuple<size_t, float> t = MidElement(points, dim);
    size_t arg_mid_val = std::get<0>(t);
    float mid_val = std::get<1>(t);

    tree_node *node = Malloc(tree_node, 1);
    node->left = nullptr;
    node->right = nullptr;
    node->id = arg_mid_val;
    node->split = dim;
    std::vector<size_t> left, right;
    for (auto &i : points) {
        if (i == arg_mid_val)
            continue;
        if (GetDimVal(i, dim) <= mid_val)
            left.emplace_back(i);
        else
            right.emplace_back(i);
    }
    if (!left.empty())
        node->left = BuildTree(left);
    if (!right.empty())
        node->right = BuildTree(right);
    return node;
}

std::tuple<size_t, float> KDTree::MidElement(const std::vector<size_t> &points, size_t dim) {
    size_t len = points.size();
    for (size_t i = 0; i < points.size(); ++i)
        get_mid_buf_[i] = std::make_tuple(points[i], GetDimVal(points[i], dim));
    std::nth_element(get_mid_buf_,
                     get_mid_buf_ + len / 2,
                     get_mid_buf_ + len,
                     [](const std::tuple<size_t, float> &i, const std::tuple<size_t, float> &j) {
                         return std::get<1>(i) < std::get<1>(j);
                     });
    return get_mid_buf_[len / 2];
}


inline void KDTree::HeapStackPush(std::stack<tree_node *> &paths, tree_node *node, const float *coor, size_t k) {
    paths.emplace(node);
    size_t id = node->id;
    if (visited_buf_[id])
        return;
    visited_buf_[id] = true;
    float dist = GetDist(id, coor);
    std::tuple<size_t, float> t(id, dist);
    if (k_neighbor_heap_.size() < k)
        k_neighbor_heap_.push(t);

    else if (std::get<1>(t) < std::get<1>(k_neighbor_heap_.top())) {
        k_neighbor_heap_.pop();
        k_neighbor_heap_.push(t);
    }
}

#ifdef USE_INTEL_MKL

inline float KDTree::GetDist(size_t i, const float *coor) {
    size_t idx = i * n_features;
    vsSub(n_features, datas + idx, coor, mkl_buf_);
    vsPowx(n_features, mkl_buf_, p, mkl_buf_);
    float dist = cblas_sasum(n_features, mkl_buf_, 1);
    return static_cast<float>(pow(dist, 1.0 / p));
}

#else

inline float KDTree::GetDist(size_t i, const float *coor) {
    float dist = 0.0;
    size_t idx = i * n_features;
#pragma omp parallel for reduction(+:dist)
    for (int t = 0; t < n_features; ++t)
        dist += pow(datas[idx + t] - coor[t], p);
    return static_cast<float>(pow(dist, 1.0 / p));
}

#endif

size_t KDTree::FindSplitDim(const std::vector<size_t> &points) {
    if (points.size() == 1)
        return 0;
    size_t cur_best_dim = 0;
    float cur_largest_spread = -1;
    float cur_min_val;
    float cur_max_val;
    for (size_t dim = 0; dim < n_features; ++dim) {
        cur_min_val = GetDimVal(points[0], dim);
        cur_max_val = GetDimVal(points[0], dim);
        for (const auto &id : points) {
            if (GetDimVal(id, dim) > cur_max_val)
                cur_max_val = GetDimVal(id, dim);
            else if (GetDimVal(id, dim) < cur_min_val)
                cur_min_val = GetDimVal(id, dim);
        }

        if (cur_max_val - cur_min_val > cur_largest_spread) {
            cur_largest_spread = cur_max_val - cur_min_val;
            cur_best_dim = dim;
        }
    }
    return cur_best_dim;
}