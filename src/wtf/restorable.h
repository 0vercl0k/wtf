// Axel '0vercl0k' Souchet - March 27 2020
#pragma once

class Restorable {
  virtual void Save() = 0;
  virtual void Restore() = 0;
};