#pragma once

struct AppContext;

class Bootstrapper {
public:
    /// Creates a fully-populated AppContext with all engine shared_ptrs.
    static AppContext createContext();
};
