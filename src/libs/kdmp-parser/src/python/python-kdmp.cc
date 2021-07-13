// Mastho - 2020
// Axel '0vercl0k' Souchet - December 27 2020
#include "python-kdmp.h"

//
// Python Dump instance creation (allocate and initialize kernel dump object).
//   >>> Dump(filepath)
//

PyObject *NewDumpParser(PyTypeObject *Type, PyObject *Args, PyObject *) {

  //
  // Allocate and zero PythonDumpParser.
  //

  auto *Self = PyObject_New(PythonDumpParser, Type);
  Self->DumpParser = nullptr;

  //
  // Parse Python argument (expect a string i.e. the file path of the dump).
  //    *PyErr_Format returns nullptr to raise the exception*
  //

  char *DumpPath = nullptr;
  if (!PyArg_ParseTuple(Args, "s", &DumpPath)) {
    DeleteDumpParser((PyObject *)Self);
    return PyErr_Format(PyExc_TypeError, "Dump() expected a string");
  }

  //
  // Initialize the internal KernelDumpParser and validate the dump file.
  //

  Self->DumpParser = new kdmpparser::KernelDumpParser();
  if (!Self->DumpParser->Parse(DumpPath)) {
    DeleteDumpParser((PyObject *)Self);
    return PyErr_Format(PyExc_ValueError, "Dump() invalid path");
  }

  //
  // Return the new instance of PythonDumpParser to Python.
  //

  return (PyObject *)Self;
}

//
// Python Dump instance destruction.
//   >>> del dump_instance
//

void DeleteDumpParser(PyObject *Object) {

  //
  // Release internal KernelDumpParser object.
  //

  auto *Self = (PythonDumpParser *)Object;

  if (Self->DumpParser) {
    delete Self->DumpParser;
    Self->DumpParser = nullptr;
  }

  //
  // Free type reference and self.
  //

  PyObject_Del(Object);
}

//
// Python Dump instance method to retrieve the DumpType.
//  >>> dump_instance.type() # return int
//

PyObject *DumpParserGetType(PyObject *Object, PyObject *) {

  //
  // Get the dump type (FullDump, KernelDump or BMPDump).
  //

  const auto *Self = (PythonDumpParser *)Object;
  const auto DumpType = Self->DumpParser->GetDumpType();
  return PyLong_FromUnsignedLong(static_cast<unsigned long>(DumpType));
}

//
//  Python Dump instance method to retrieve the register context.
//  >>> dump_instance.context() # return dict(str -> int)
//

PyObject *DumpParserGetContext(PyObject *Object, PyObject *) {

  //
  // Get the dump context (commons registers).
  //

  const auto *Self = (PythonDumpParser *)Object;
  const auto *C = Self->DumpParser->GetContext();

  //
  // Create a Python dict object with lowercase register name and value.
  //

  PyObject *Context = PyDict_New();

  PyDict_SetItemString(Context, "rax", PyLong_FromUnsignedLongLong(C->Rax));
  PyDict_SetItemString(Context, "rbx", PyLong_FromUnsignedLongLong(C->Rbx));
  PyDict_SetItemString(Context, "rcx", PyLong_FromUnsignedLongLong(C->Rcx));
  PyDict_SetItemString(Context, "rdx", PyLong_FromUnsignedLongLong(C->Rdx));
  PyDict_SetItemString(Context, "rsi", PyLong_FromUnsignedLongLong(C->Rsi));
  PyDict_SetItemString(Context, "rdi", PyLong_FromUnsignedLongLong(C->Rdi));
  PyDict_SetItemString(Context, "rip", PyLong_FromUnsignedLongLong(C->Rip));
  PyDict_SetItemString(Context, "rsp", PyLong_FromUnsignedLongLong(C->Rsp));
  PyDict_SetItemString(Context, "rbp", PyLong_FromUnsignedLongLong(C->Rbp));
  PyDict_SetItemString(Context, "r8", PyLong_FromUnsignedLongLong(C->R8));
  PyDict_SetItemString(Context, "r9", PyLong_FromUnsignedLongLong(C->R9));
  PyDict_SetItemString(Context, "r10", PyLong_FromUnsignedLongLong(C->R10));
  PyDict_SetItemString(Context, "r11", PyLong_FromUnsignedLongLong(C->R11));
  PyDict_SetItemString(Context, "r12", PyLong_FromUnsignedLongLong(C->R12));
  PyDict_SetItemString(Context, "r13", PyLong_FromUnsignedLongLong(C->R13));
  PyDict_SetItemString(Context, "r14", PyLong_FromUnsignedLongLong(C->R14));
  PyDict_SetItemString(Context, "r15", PyLong_FromUnsignedLongLong(C->R15));

  //
  //  Get the DirectoryTableBase from the dump and return the created dict to
  //  Python.
  //
  PyDict_SetItemString(
      Context, "dtb",
      PyLong_FromUnsignedLongLong(Self->DumpParser->GetDirectoryTableBase()));

  return Context;
}

//
//  Python Dump instance method to retrieve the bugcheck parameters.
//  >>> dump_instance.bugcheck() # return dict
//

PyObject *DumpParserGetBugCheckParameters(PyObject *Object, PyObject *) {

  //
  // Retrieve the bugcheck parameters.
  //

  const auto *Self = (PythonDumpParser *)Object;
  const auto Parameters = Self->DumpParser->GetBugCheckParameters();

  const uint64_t NumberParams = sizeof(Parameters.BugCheckCodeParameter) /
                                sizeof(Parameters.BugCheckCodeParameter[0]);
  PyObject *PythonParamsList = PyList_New(NumberParams);

  for (uint64_t Idx = 0; Idx < NumberParams; Idx++) {
    PyList_SetItem(
        PythonParamsList, Idx,
        PyLong_FromUnsignedLongLong(Parameters.BugCheckCodeParameter[Idx]));
  }

  //
  // Create a Python dict object with code and parameters.
  //

  PyObject *PythonParams = PyDict_New();

  PyDict_SetItemString(PythonParams, "code",
                       PyLong_FromUnsignedLong(Parameters.BugCheckCode));
  PyDict_SetItemString(PythonParams, "parameters", PythonParamsList);

  return PythonParams;
}

//
//  Python Dump instance method to get a physical page from a physical address.
//  >>> dump_instance.get_physical_page(addr) # return bytes
//

PyObject *DumpParserGetPhysicalPage(PyObject *Object, PyObject *Args) {

  //
  // Parse Python argument (expect one unsigned long long integer).
  //

  const auto *Self = (PythonDumpParser *)Object;

  uint64_t PhysicalAddress = 0;
  if (!PyArg_ParseTuple(Args, "K", &PhysicalAddress)) {
    return PyErr_Format(PyExc_TypeError,
                        "get_physical_page() expected an integer");
  }

  //
  // Get the physical page and return it as bytes.
  //

  const uint8_t *Page = Self->DumpParser->GetPhysicalPage(PhysicalAddress);
  if (Page == nullptr) {
    return PyErr_Format(PyExc_ValueError,
                        "get_physical_page() invalid address");
  }

  return PyBytes_FromStringAndSize((char *)Page, kdmpparser::Page::Size);
}

//
//  Python Dump instance method to perform address translation (physical to
//  virtual).
//  >>> dump_instance.virt_translate(addr, [dtb]) # return int
//

PyObject *DumpParserVirtTranslate(PyObject *Object, PyObject *Args) {

  //
  // Parse Python argument (expect one or two unsigned long long integer).
  //

  const auto *Self = (PythonDumpParser *)Object;

  uint64_t VirtualAddress = 0;
  uint64_t DirectoryTableBase = 0;
  if (!PyArg_ParseTuple(Args, "K|K", &VirtualAddress, &DirectoryTableBase)) {
    return PyErr_Format(PyExc_TypeError,
                        "virt_translate() expected one or two integers");
  }

  //
  // Retrieve the physical address (parse pages tables in the dump).
  //

  const auto &PhysicalAddress =
      Self->DumpParser->VirtTranslate(VirtualAddress, DirectoryTableBase);

  if (!PhysicalAddress) {
    return PyErr_Format(PyExc_ValueError, "virt_translate() invalid address");
  }

  return PyLong_FromUnsignedLongLong(*PhysicalAddress);
}

//
//  Python Dump instance method to get a page from a virtual address.
//  >>> dump_instance.get_virtual_page(addr, [dtb]) # return bytes
//

PyObject *DumpParserGetVirtualPage(PyObject *Object, PyObject *Args) {

  //
  // Parse Python argument (expect one or two unsigned long long integer).
  //

  const auto *Self = (PythonDumpParser *)Object;

  uint64_t VirtualAddress = 0;
  uint64_t DirectoryTableBase = 0;
  if (!PyArg_ParseTuple(Args, "K|K", &VirtualAddress, &DirectoryTableBase)) {
    return PyErr_Format(PyExc_TypeError,
                        "get_virtual_page() expected one or two integer");
  }

  const uint8_t *Page =
      Self->DumpParser->GetVirtualPage(VirtualAddress, DirectoryTableBase);
  if (Page == nullptr) {
    return PyErr_Format(PyExc_ValueError, "get_virtual_page() invalid address");
  }

  return PyBytes_FromStringAndSize((char *)Page, kdmpparser::Page::Size);
}

//
// KDMP Module initialization function.
//

PyMODINIT_FUNC PyInit_kdmp() {

  //
  // Initialize python.
  //

  Py_Initialize();

  //
  // Expose the kdmp module.
  //

  PyObject *M = PyModule_Create(&KDMPModule);
  if (M == nullptr) {
    return nullptr;
  }

  //
  // Register PythonDumpParserType.
  //

  PyObject *Ty = PyType_FromSpec(&TySpec);
  if (Ty == nullptr) {
    return nullptr;
  }

  KDMPState(M)->PythonDumpParserType = (PyTypeObject *)Ty;

  //
  // Expose the PythonDumpParserType to Python in kdmp module.
  //  >>> kdmp.Dump class
  //

  Py_INCREF(Ty);
  if (PyModule_AddObject(M, "Dump", Ty) < 0) {
    Py_DECREF(Ty);
    Py_DECREF(M);
    return nullptr;
  }

  //
  // Expose DumpType constants to Python.
  //  >>> kdmp.FullDump ...
  //

  PyModule_AddIntConstant(M, "FullDump",
                          long(kdmpparser::DumpType_t::FullDump));
  PyModule_AddIntConstant(M, "KernelDump",
                          long(kdmpparser::DumpType_t::KernelDump));
  PyModule_AddIntConstant(M, "BMPDump", long(kdmpparser::DumpType_t::BMPDump));
  return M;
}
