/*
 * gcl_pygcl.c — GCL embed Python helper module (`gcl`).
 *
 * Used on the Python side with `import gcl`:
 *   import gcl
 *   gcl.init()   → no-op (application startup)
 *
 * Python C extension module: PyInit_gcl (gcl.pyd|.so).
 */

#include <Python.h>

#include "gcl_shared_state.h"

#ifdef _WIN32
#define GCL_PY_EXPORT __declspec(dllexport)
#else
#define GCL_PY_EXPORT __attribute__((visibility("default")))
#endif

static PyObject *py_gcl_init(PyObject *self, PyObject *args) {
    (void)self; (void)args;
    Py_RETURN_NONE;
}

static PyObject *py_gcl_get_state(PyObject *self, PyObject *args) {
    (void)self;
    const char *key;
    if (!PyArg_ParseTuple(args, "s", &key))
        return NULL;
    char buf[1024] = { 0 };
    if (gcl_shared_get(key, buf, sizeof(buf)))
        return PyFloat_FromDouble(atof(buf));
    Py_RETURN_NONE;
}

static PyObject *py_gcl_set_state(PyObject *self, PyObject *args) {
    (void)self;
    const char *key;
    double val;
    if (!PyArg_ParseTuple(args, "sd", &key, &val))
        return NULL;
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", val);
    gcl_shared_set(key, buf);
    Py_RETURN_NONE;
}

static PyMethodDef gcl_methods[] = {
    {"init", py_gcl_init, METH_NOARGS, "GCL init (no-op)"},
    {"get_state", py_gcl_get_state, METH_VARARGS, "Read shared cross-process state"},
    {"set_state", py_gcl_set_state, METH_VARARGS, "Write shared cross-process state"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef gcl_module = {
    PyModuleDef_HEAD_INIT,
    "gcl",
    "GCL embedded helper",
    -1,
    gcl_methods
};

GCL_PY_EXPORT PyObject *PyInit_gcl(void) {
    return PyModule_Create(&gcl_module);
}
