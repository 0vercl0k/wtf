// Mastho - 2020
// Axel '0vercl0k' Souchet - December 27 2020
#pragma once

//
// PEP-384
// """
// Applications shall only include the header file Python.h (before including
// any system headers).
//

#define Py_LIMITED_API
#include <Python.h>

#include "kdmp-parser.h"

//
// Python object handling all interactions with the library.
//

struct PythonDumpParser {
  PyObject_HEAD kdmpparser::KernelDumpParser *DumpParser = nullptr;
};

//
// Python Dump type functions declarations (class instance creation and instance
// destruction).
//

PyObject *NewDumpParser(PyTypeObject *Type, PyObject *Args, PyObject *Kwds);
void DeleteDumpParser(PyObject *Object);

//
// Python Dump object methods functions declarations.
//

PyObject *DumpParserGetType(PyObject *Object, PyObject *);
PyObject *DumpParserGetContext(PyObject *Object, PyObject *);
PyObject *DumpParserGetPhysicalPage(PyObject *Object, PyObject *Args);
PyObject *DumpParserVirtTranslate(PyObject *Object, PyObject *Args);
PyObject *DumpParserGetVirtualPage(PyObject *Object, PyObject *Args);
PyObject *DumpParserGetBugCheckParameters(PyObject *Object, PyObject *);

//
// Object methods of Python Dump type.
//

PyMethodDef DumpObjectMethod[] = {
    {"type", DumpParserGetType, METH_NOARGS,
     "Show Dump Type (FullDump, KernelDump, BMPDump)"},
    {"context", DumpParserGetContext, METH_NOARGS, "Get Register Context"},
    {"get_physical_page", DumpParserGetPhysicalPage, METH_VARARGS,
     "Get Physical Page Content"},
    {"virt_translate", DumpParserVirtTranslate, METH_VARARGS,
     "Translate Virtual to Physical Address"},
    {"get_virtual_page", DumpParserGetVirtualPage, METH_VARARGS,
     "Get Virtual Page Content"},
    {"bugcheck", DumpParserGetBugCheckParameters, METH_NOARGS,
     "Get BugCheck Parameters"},
    {nullptr, nullptr, 0, nullptr}};

//
// Define Slots/Spec (name, initialization & destruction
// functions and object methods).
//

PyType_Slot TySlots[] = {
    {Py_tp_doc, (void *)"Dump object"},
    {Py_tp_new, (void *)NewDumpParser},
    {Py_tp_dealloc, (void *)DeleteDumpParser},
    {Py_tp_methods, DumpObjectMethod},
    {0, 0},
};

PyType_Spec TySpec = {"kdmp.Dump", sizeof(PythonDumpParser), 0,
                      Py_TPFLAGS_DEFAULT, TySlots};

//
// KDMP Module definition.
//

struct KDMPState {
  PyTypeObject *PythonDumpParserType = nullptr;
};

struct PyModuleDef KDMPModule = {
    PyModuleDef_HEAD_INIT, /* m_base */
    "kdmp",                /* m_name */
    "KDMP module",         /* m_doc */
    sizeof(KDMPState),     /* m_size */
    nullptr,               /* m_methods */
    nullptr,               /* m_slots */
    nullptr,               /* m_traverse */
    nullptr,               /* m_clear */
    nullptr,               /* m_free */
};

//
// KDMP Module initialization function.
//

PyMODINIT_FUNC PyInit_kdmp();

#define KDMPState(o) ((KDMPState *)PyModule_GetState(o))
