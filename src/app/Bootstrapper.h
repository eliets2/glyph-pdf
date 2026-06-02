// SPDX-License-Identifier: Apache-2.0
#pragma once

struct AppContext;

class Bootstrapper {
public:
    /// Creates a fully-populated AppContext with all engine shared_ptrs.
    static AppContext createContext();
};
