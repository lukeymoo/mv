#pragma once

// Contains copy of all relevant states for the last frame
class StateMachine
{
  StateMachine (void);
  ~StateMachine ();

  // Performs a copy on parameter
  template <typename T>
  void Submit (T);
};