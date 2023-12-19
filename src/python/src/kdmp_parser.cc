//
// This file is part of kdmp-parser project
//
// Released under MIT License, by 0vercl0k - 2023
//
// With contributions from:
//  * hugsy - (github.com/hugsy)
//

#include "kdmp-parser.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/variant.h>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(_kdmp_parser, m) {

  m.doc() = "KDMP parser module";

  nb::class_<kdmpparser::Version_t>(m, "version")
      .def_ro_static("major", &kdmpparser::Version_t::Major)
      .def_ro_static("minor", &kdmpparser::Version_t::Minor)
      .def_ro_static("patch", &kdmpparser::Version_t::Patch)
      .def_ro_static("release", &kdmpparser::Version_t::Release);

  nb::class_<kdmpparser::uint128_t>(m, "uint128_t")
      .def(nb::init<>())
      .def_rw("Low", &kdmpparser::uint128_t::Low)
      .def_rw("High", &kdmpparser::uint128_t::High);

  nb::enum_<kdmpparser::DumpType_t>(m, "DumpType_t")
      .value("FullDump", kdmpparser::DumpType_t::FullDump)
      .value("KernelDump", kdmpparser::DumpType_t::KernelDump)
      .value("BMPDump", kdmpparser::DumpType_t::BMPDump)

      .value("MiniDump", kdmpparser::DumpType_t::MiniDump)
      .value("KernelMemoryDump", kdmpparser::DumpType_t::KernelMemoryDump)
      .value("KernelAndUserMemoryDump",
             kdmpparser::DumpType_t::KernelAndUserMemoryDump)
      .value("CompleteMemoryDump", kdmpparser::DumpType_t::CompleteMemoryDump)
      .export_values();

  nb::class_<kdmpparser::PHYSMEM_RUN>(m, "PHYSMEM_RUN")
      .def(nb::init<>())
      .def_rw("BasePage", &kdmpparser::PHYSMEM_RUN::BasePage)
      .def_rw("PageCount", &kdmpparser::PHYSMEM_RUN::PageCount)
      .def("Show", &kdmpparser::PHYSMEM_RUN::Show, "Prefix"_a);

  nb::class_<kdmpparser::PHYSMEM_DESC>(m, "PHYSMEM_DESC")
      .def(nb::init<>())
      .def_ro("NumberOfRuns", &kdmpparser::PHYSMEM_DESC::NumberOfRuns)
      .def_ro("Padding0", &kdmpparser::PHYSMEM_DESC::Padding0)
      .def_ro("NumberOfPages", &kdmpparser::PHYSMEM_DESC::NumberOfPages)
      .def_ro("Run", &kdmpparser::PHYSMEM_DESC::Run)
      .def("Show", &kdmpparser::PHYSMEM_DESC::Show, "Prefix"_a)
      .def("LooksGood", &kdmpparser::PHYSMEM_DESC::LooksGood);

  nb::class_<kdmpparser::BMP_HEADER64>(m, "BMP_HEADER64")
      .def(nb::init<>())
      .def_ro_static("ExpectedSignature",
                     &kdmpparser::BMP_HEADER64::ExpectedSignature)
      .def_ro_static("ExpectedSignature2",
                     &kdmpparser::BMP_HEADER64::ExpectedSignature2)
      .def_ro_static("ExpectedValidDump",
                     &kdmpparser::BMP_HEADER64::ExpectedValidDump)
      .def_ro("Signature", &kdmpparser::BMP_HEADER64::Signature)
      .def_ro("ValidDump", &kdmpparser::BMP_HEADER64::ValidDump)
      .def_ro("Padding0", &kdmpparser::BMP_HEADER64::Padding0)
      .def_ro("FirstPage", &kdmpparser::BMP_HEADER64::FirstPage)
      .def_ro("TotalPresentPages", &kdmpparser::BMP_HEADER64::TotalPresentPages)
      .def_ro("Pages", &kdmpparser::BMP_HEADER64::Pages)
      .def_ro("Bitmap", &kdmpparser::BMP_HEADER64::Bitmap)
      .def("Show", &kdmpparser::PHYSMEM_DESC::Show, "Prefix"_a)
      .def("LooksGood", &kdmpparser::PHYSMEM_DESC::LooksGood);

  nb::class_<kdmpparser::RDMP_HEADER64>(m, "RDMP_HEADER64")
      .def(nb::init<>())
      .def_ro_static("ExpectedMarker",
                     &kdmpparser::RDMP_HEADER64::ExpectedMarker)
      .def_ro_static("ExpectedSignature",
                     &kdmpparser::RDMP_HEADER64::ExpectedSignature)
      .def_ro_static("ExpectedValidDump",
                     &kdmpparser::RDMP_HEADER64::ExpectedValidDump)
      .def_ro("Marker", &kdmpparser::RDMP_HEADER64::Marker)
      .def_ro("Signature", &kdmpparser::RDMP_HEADER64::Signature)
      .def_ro("ValidDump", &kdmpparser::RDMP_HEADER64::ValidDump)
      .def_ro("MetadataSize", &kdmpparser::RDMP_HEADER64::MetadataSize)
      .def_ro("FirstPageOffset", &kdmpparser::RDMP_HEADER64::FirstPageOffset)
      .def("LooksGood", &kdmpparser::RDMP_HEADER64::LooksGood)
      .def("Show", &kdmpparser::RDMP_HEADER64::Show);

  nb::class_<kdmpparser::KERNEL_RDMP_HEADER64>(m, "KERNEL_RDMP_HEADER64")
      .def(nb::init<>());

  nb::class_<kdmpparser::FULL_RDMP_HEADER64>(m, "FULL_RDMP_HEADER64")
      .def(nb::init<>());

  using CONTEXT = kdmpparser::CONTEXT;
  nb::class_<CONTEXT>(m, "CONTEXT")
      .def(nb::init<>())
      .def_ro("P1Home", &CONTEXT::P1Home)
      .def_ro("P2Home", &CONTEXT::P2Home)
      .def_ro("P3Home", &CONTEXT::P3Home)
      .def_ro("P4Home", &CONTEXT::P4Home)
      .def_ro("P5Home", &CONTEXT::P5Home)
      .def_ro("P6Home", &CONTEXT::P6Home)
      .def_ro("ContextFlags", &CONTEXT::ContextFlags)
      .def_ro("MxCsr", &CONTEXT::MxCsr)
      .def_ro("SegCs", &CONTEXT::SegCs)
      .def_ro("SegDs", &CONTEXT::SegDs)
      .def_ro("SegEs", &CONTEXT::SegEs)
      .def_ro("SegFs", &CONTEXT::SegFs)
      .def_ro("SegGs", &CONTEXT::SegGs)
      .def_ro("SegSs", &CONTEXT::SegSs)
      .def_ro("EFlags", &CONTEXT::EFlags)
      .def_ro("Dr0", &CONTEXT::Dr0)
      .def_ro("Dr1", &CONTEXT::Dr1)
      .def_ro("Dr2", &CONTEXT::Dr2)
      .def_ro("Dr3", &CONTEXT::Dr3)
      .def_ro("Dr6", &CONTEXT::Dr6)
      .def_ro("Dr7", &CONTEXT::Dr7)
      .def_ro("Rax", &CONTEXT::Rax)
      .def_ro("Rcx", &CONTEXT::Rcx)
      .def_ro("Rdx", &CONTEXT::Rdx)
      .def_ro("Rbx", &CONTEXT::Rbx)
      .def_ro("Rsp", &CONTEXT::Rsp)
      .def_ro("Rbp", &CONTEXT::Rbp)
      .def_ro("Rsi", &CONTEXT::Rsi)
      .def_ro("Rdi", &CONTEXT::Rdi)
      .def_ro("R8", &CONTEXT::R8)
      .def_ro("R9", &CONTEXT::R9)
      .def_ro("R10", &CONTEXT::R10)
      .def_ro("R11", &CONTEXT::R11)
      .def_ro("R12", &CONTEXT::R12)
      .def_ro("R13", &CONTEXT::R13)
      .def_ro("R14", &CONTEXT::R14)
      .def_ro("R15", &CONTEXT::R15)
      .def_ro("Rip", &CONTEXT::Rip)
      .def_ro("ControlWord", &CONTEXT::ControlWord)
      .def_ro("StatusWord", &CONTEXT::StatusWord)
      .def_ro("TagWord", &CONTEXT::TagWord)
      .def_ro("Reserved1", &CONTEXT::Reserved1)
      .def_ro("ErrorOpcode", &CONTEXT::ErrorOpcode)
      .def_ro("ErrorOffset", &CONTEXT::ErrorOffset)
      .def_ro("ErrorSelector", &CONTEXT::ErrorSelector)
      .def_ro("Reserved2", &CONTEXT::Reserved2)
      .def_ro("DataOffset", &CONTEXT::DataOffset)
      .def_ro("DataSelector", &CONTEXT::DataSelector)
      .def_ro("Reserved3", &CONTEXT::Reserved3)
      .def_ro("MxCsr2", &CONTEXT::MxCsr2)
      .def_ro("MxCsr_Mask", &CONTEXT::MxCsr_Mask)
      .def_ro("FloatRegisters", &CONTEXT::FloatRegisters)
      .def_ro("Xmm0", &CONTEXT::Xmm0)
      .def_ro("Xmm1", &CONTEXT::Xmm1)
      .def_ro("Xmm2", &CONTEXT::Xmm2)
      .def_ro("Xmm3", &CONTEXT::Xmm3)
      .def_ro("Xmm4", &CONTEXT::Xmm4)
      .def_ro("Xmm5", &CONTEXT::Xmm5)
      .def_ro("Xmm6", &CONTEXT::Xmm6)
      .def_ro("Xmm7", &CONTEXT::Xmm7)
      .def_ro("Xmm8", &CONTEXT::Xmm8)
      .def_ro("Xmm9", &CONTEXT::Xmm9)
      .def_ro("Xmm10", &CONTEXT::Xmm10)
      .def_ro("Xmm11", &CONTEXT::Xmm11)
      .def_ro("Xmm12", &CONTEXT::Xmm12)
      .def_ro("Xmm13", &CONTEXT::Xmm13)
      .def_ro("Xmm14", &CONTEXT::Xmm14)
      .def_ro("Xmm15", &CONTEXT::Xmm15)
      .def_ro("VectorRegister", &CONTEXT::VectorRegister)
      .def_ro("VectorControl", &CONTEXT::VectorControl)
      .def_ro("DebugControl", &CONTEXT::DebugControl)
      .def_ro("LastBranchToRip", &CONTEXT::LastBranchToRip)
      .def_ro("LastBranchFromRip", &CONTEXT::LastBranchFromRip)
      .def_ro("LastExceptionToRip", &CONTEXT::LastExceptionToRip)
      .def_ro("LastExceptionFromRip", &CONTEXT::LastExceptionFromRip)
      .def("Show", &CONTEXT::Show, "Prefix"_a)
      .def("LooksGood", &CONTEXT::LooksGood);

  using EXCEPTION_RECORD64 = kdmpparser::EXCEPTION_RECORD64;
  nb::class_<EXCEPTION_RECORD64>(m, "EXCEPTION_RECORD64")
      .def(nb::init<>())
      .def_ro("ExceptionCode", &EXCEPTION_RECORD64::ExceptionCode)
      .def_ro("ExceptionFlags", &EXCEPTION_RECORD64::ExceptionFlags)
      .def_ro("ExceptionRecord", &EXCEPTION_RECORD64::ExceptionRecord)
      .def_ro("ExceptionAddress", &EXCEPTION_RECORD64::ExceptionAddress)
      .def_ro("NumberParameters", &EXCEPTION_RECORD64::NumberParameters)
      .def_ro("__unusedAlignment", &EXCEPTION_RECORD64::__unusedAlignment)
      .def_ro("ExceptionInformation", &EXCEPTION_RECORD64::ExceptionInformation)
      .def("Show", &EXCEPTION_RECORD64::Show, "Prefix"_a);

  using HEADER64 = kdmpparser::HEADER64;
  nb::class_<HEADER64>(m, "HEADER64")
      .def(nb::init<>())
      .def_ro_static("ExpectedSignature", &HEADER64::ExpectedSignature)
      .def_ro_static("ExpectedValidDump", &HEADER64::ExpectedValidDump)
      .def_ro("Signature", &HEADER64::Signature)
      .def_ro("ValidDump", &HEADER64::ValidDump)
      .def_ro("MajorVersion", &HEADER64::MajorVersion)
      .def_ro("MinorVersion", &HEADER64::MinorVersion)
      .def_ro("DirectoryTableBase", &HEADER64::DirectoryTableBase)
      .def_ro("PfnDatabase", &HEADER64::PfnDatabase)
      .def_ro("PsLoadedModuleList", &HEADER64::PsLoadedModuleList)
      .def_ro("PsActiveProcessHead", &HEADER64::PsActiveProcessHead)
      .def_ro("MachineImageType", &HEADER64::MachineImageType)
      .def_ro("NumberProcessors", &HEADER64::NumberProcessors)
      .def_ro("BugCheckCode", &HEADER64::BugCheckCode)
      .def_ro("BugCheckCodeParameter", &HEADER64::BugCheckCodeParameters)
      .def_ro("KdDebuggerDataBlock", &HEADER64::KdDebuggerDataBlock)
      .def_prop_ro("PhysicalMemoryBlock",
                   [](const HEADER64 &Hdr) -> kdmpparser::PHYSMEM_DESC {
                     return Hdr.u1.PhysicalMemoryBlock;
                   })
      .def_prop_ro("ContextRecord",
                   [](const HEADER64 &Hdr) { return Hdr.u2.ContextRecord; })
      .def_ro("Exception", &HEADER64::Exception)
      .def_ro("DumpType", &HEADER64::DumpType)
      .def_ro("RequiredDumpSpace", &HEADER64::RequiredDumpSpace)
      .def_ro("SystemTime", &HEADER64::SystemTime)
      .def_ro("Comment", &HEADER64::Comment)
      .def_ro("SystemUpTime", &HEADER64::SystemUpTime)
      .def_ro("MiniDumpFields", &HEADER64::MiniDumpFields)
      .def_ro("SecondaryDataState", &HEADER64::SecondaryDataState)
      .def_ro("ProductType", &HEADER64::ProductType)
      .def_ro("SuiteMask", &HEADER64::SuiteMask)
      .def_ro("WriterStatus", &HEADER64::WriterStatus)
      .def_ro("KdSecondaryVersion", &HEADER64::KdSecondaryVersion)
      .def_ro("Attributes", &HEADER64::Attributes)
      .def_ro("BootId", &HEADER64::BootId)
      .def_prop_ro(
          "BmpHeader",
          [](const HEADER64 &Hdr) -> std::optional<kdmpparser::BMP_HEADER64> {
            if (Hdr.DumpType != kdmpparser::DumpType_t::BMPDump) {
              return {};
            }

            return Hdr.u3.BmpHeader;
          })
      .def_prop_ro("RdmpHeader",
                   [](const HEADER64 &Hdr)
                       -> std::optional<kdmpparser::KERNEL_RDMP_HEADER64> {
                     if (Hdr.DumpType !=
                             kdmpparser::DumpType_t::KernelAndUserMemoryDump &&
                         Hdr.DumpType !=
                             kdmpparser::DumpType_t::KernelMemoryDump) {
                       return {};
                     }

                     return Hdr.u3.RdmpHeader;
                   })
      .def_prop_ro("FullRdmpHeader",
                   [](const HEADER64 &Hdr)
                       -> std::optional<kdmpparser::FULL_RDMP_HEADER64> {
                     if (Hdr.DumpType !=
                         kdmpparser::DumpType_t::CompleteMemoryDump) {
                       return {};
                     }

                     return Hdr.u3.FullRdmpHeader;
                   })
      .def("Show", &CONTEXT::Show, "Prefix"_a)
      .def("LooksGood", &CONTEXT::LooksGood);

  m.attr("PageSize") = kdmpparser::Page::Size;
  m.def("PageAlign", &kdmpparser::Page::Align, "Address"_a,
        "Get the aligned value on the page for the given address.");
  m.def("PageOffset", &kdmpparser::Page::Offset, "Address"_a,
        "Get the offset to the page for the given address.");

  using BugCheckParameters_t = kdmpparser::BugCheckParameters_t;
  nb::class_<BugCheckParameters_t>(m, "BugCheckParameters_t")
      .def(nb::init<>())
      .def_ro("BugCheckCode", &BugCheckParameters_t::BugCheckCode)
      .def_ro("BugCheckCodeParameter",
              &BugCheckParameters_t::BugCheckCodeParameter);

  using KernelDumpParser = kdmpparser::KernelDumpParser;
  nb::class_<KernelDumpParser>(m, "KernelDumpParser")
      .def(nb::init<>())
      .def("Parse", &KernelDumpParser::Parse, "PathFile"_a)
      .def("GetContext", &KernelDumpParser::GetContext)
      .def("GetDumpHeader", &KernelDumpParser::GetDumpHeader,
           nb::rv_policy::reference)
      .def("GetBugCheckParameters", &KernelDumpParser::GetBugCheckParameters)
      .def("GetDumpType", &KernelDumpParser::GetDumpType)
      .def("GetPhysmem",
           [](const KernelDumpParser &Parser) {
             const auto &PhysMem = Parser.GetPhysmem();
             return nb::make_key_iterator(nb::type<std::vector<uint64_t>>(),
                                          "it", PhysMem.cbegin(),
                                          PhysMem.cend());
           })
      .def("ShowExceptionRecord", &KernelDumpParser::ShowExceptionRecord,
           "Prefix"_a = 0)
      .def("ShowContextRecord", &KernelDumpParser::ShowContextRecord,
           "Prefix"_a = 0)
      .def("ShowAllStructures", &KernelDumpParser::ShowAllStructures,
           "Prefix"_a = 0)
      .def(
          "GetPhysicalPage",
          [](const KernelDumpParser &Parser, const uint64_t PhysicalAddress)
              -> std::optional<kdmpparser::Page_t> {
            const auto *Page = Parser.GetPhysicalPage(PhysicalAddress);
            if (!Page) {
              return std::nullopt;
            }

            kdmpparser::Page_t Out;
            memcpy(Out.data(), Page, kdmpparser::Page::Size);
            return Out;
          },
          "PhysicalAddress"_a)
      .def("GetDirectoryTableBase", &KernelDumpParser::GetDirectoryTableBase)
      .def("VirtTranslate", &KernelDumpParser::VirtTranslate,
           "VirtualAddress"_a, "DirectoryTableBase"_a)
      .def(
          "GetVirtualPage",
          [](const KernelDumpParser &Parser, const uint64_t VirtualAddress,
             const uint64_t DirectoryTableBase =
                 0) -> std::optional<kdmpparser::Page_t> {
            const auto *Page =
                Parser.GetVirtualPage(VirtualAddress, DirectoryTableBase);
            if (!Page) {
              return std::nullopt;
            }

            kdmpparser::Page_t Out;
            memcpy(Out.data(), Page, kdmpparser::Page::Size);
            return Out;
          },
          "VirtualAddress"_a, "DirectoryTableBase"_a = 0);
}
