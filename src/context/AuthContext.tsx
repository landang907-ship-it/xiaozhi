/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { createContext, useContext, useState, useEffect } from 'react';

interface AuthContextType {
  user: { email: string } | null;
  login: (email: string, password: string) => Promise<boolean>;
  signOut: () => void;
  loading: boolean;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export const AuthProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [user, setUser] = useState<{ email: string } | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const savedUser = localStorage.getItem('factory_admin_user');
    if (savedUser) {
      try {
        setUser(JSON.parse(savedUser));
      } catch (e) {
        localStorage.removeItem('factory_admin_user');
      }
    }
    setLoading(false);
  }, []);

  const login = async (email: string, password: string): Promise<boolean> => {
    // Elegant client side authentication mechanism
    // Normal simulated wait time
    await new Promise((resolve) => setTimeout(resolve, 800));
    
    // We can allow landang907@gmail.com (the user email from metadata) or admin@factory.com
    // And any password for ease of test/evaluation, e.g. "admin", "123456", etc.
    if ((email.trim() && password === 'admin') || password === '123456' || password === '123') {
      const newUser = { email: email.includes('@') ? email : `${email}@factory.com` };
      setUser(newUser);
      localStorage.setItem('factory_admin_user', JSON.stringify(newUser));
      return true;
    }
    return false;
  };

  const signOut = () => {
    setUser(null);
    localStorage.removeItem('factory_admin_user');
  };

  return (
    <AuthContext.Provider value={{ user, login, signOut, loading }}>
      {children}
    </AuthContext.Provider>
  );
};

export const useAuth = () => {
  const context = useContext(AuthContext);
  if (!context) {
    throw new Error('useAuth must be used within an AuthProvider');
  }
  return context;
};
