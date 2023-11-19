import numpy as np
from ctypes import CDLL, c_size_t, c_float, c_bool, POINTER, Structure
from abc import ABCMeta, abstractmethod
import os
import sys


DIR_NAME = os.path.dirname(os.path.abspath(__file__))
if sys.platform == 'win32':
    FILE_NAME = "kdtree.dll"
else:
    FILE_NAME = "libkdtree.so"

_lib = CDLL(os.path.join(DIR_NAME, FILE_NAME))


def fillprototype(f, restype, argtypes):
    """A helper function to decorate C-function
    Parameters
    ----------
    f: object C-function
    restype: list, return value type
    argtypes: list, arguments type
    Examples
    --------
    double add(double x, double y)
    {
        return x + y;    
    }
    fillprototype(add, [c_float, c_float])

    """
    f.restype = restype
    f.argtypes = argtypes


class tree_node(Structure):
    _fields_ = [
        ('id', c_size_t),
        ('split', c_size_t),
        ('left', POINTER(tree_node)),
        ('right', POINTER(tree_node))
    ]


class tree_model(Structure):
    _fields_ = [
        ('root', POINTER(tree_node)),
        ('datas', POINTER(c_float)),
        ('labels', POINTER(c_float)),
        ('n_samples', c_size_t),
        ('n_features', c_size_t),
        ('p', c_float)
    ]


class KNeighborsBase(metaclass=ABCMeta):
    def __init__(self, k, p=2):
        self.k = k
        self.p = p

    def __del__(self):
        if hasattr(self, "_model"):
            _lib.free_tree_memory(self._model.contents.root)

    def fit(self, X, y):
        if not isinstance(X, np.ndarray):
            raise TypeError("X must be np.ndarray")
        if not isinstance(y, np.ndarray):
            raise TypeError("y must be np.ndarray")
        if X.dtype != np.float32:
            X = X.astype(np.float32)
        if y.dtype != np.float32:
            y = y.astype(np.float32)
        self.X = X
        self.y = y
        n_samples, n_features = X.shape
        datas = X.ctypes.data_as(POINTER(c_float))
        labels = y.ctypes.data_as(POINTER(c_float))
        self._model = _lib.build_kdtree(datas, labels,
                                       n_samples, n_features, self.p)

    def _predict(self, X, is_clf):
        if not hasattr(self, "_model"):
            raise ValueError("Please call function `fit` before predict")
        if X.dtype != np.float32:
            X = X.astype(np.float32)

        test_set = X.ctypes.data_as(POINTER(c_float))
        c_arr = _lib.k_nearests_neighbor(
            self._model, test_set, X.shape[0], self.k, is_clf)
        return np.ctypeslib.as_array(c_arr, shape=(X.shape[0],))

    @abstractmethod
    def predict(self, X):
        """Predict"""


class KNeighborsClassifier(KNeighborsBase):
    def __init__(self, k, p=2):
        super().__init__(k, p)

    def predict(self, X):
        return self._predict(X, True)


class KNeighborsRegressor(KNeighborsBase):
    def __init__(self, k, p=2):
        super().__init__(k, p)

    def predict(self, X):
        return self._predict(X, False)


fillprototype(_lib.free_tree_memory, None, [POINTER(tree_node)])
fillprototype(_lib.build_kdtree,
              POINTER(tree_model), [
                  POINTER(c_float),
                  POINTER(c_float),
                  c_size_t, c_size_t, c_float
              ])
fillprototype(_lib.k_nearests_neighbor,
              POINTER(c_float), [
                  POINTER(tree_model),
                  POINTER(c_float),
                  c_size_t, c_size_t, c_bool
              ])

fillprototype(_lib.find_k_nearests, None, [
    POINTER(tree_model),
    POINTER(c_float),
    c_size_t,
    POINTER(c_size_t),
    POINTER(c_float)
])
