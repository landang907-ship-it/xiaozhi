/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import express from "express";
import path from "path";
import fs from "fs";
import crypto from "crypto";
import { createServer as createViteServer } from "vite";

import dotenv from "dotenv";
dotenv.config(); // Load .env
dotenv.config({ path: ".env.local" }); // Load .env.local if present

import { createClient } from "@supabase/supabase-js";

const DB_FILE = path.join(process.cwd(), "machines-db.json");
const HISTORY_FILE = path.join(process.cwd(), "history-db.json");
const PLAN_FILE = path.join(process.cwd(), "plan-db.json");

const supabaseUrl = process.env.SUPABASE_URL || process.env.VITE_SUPABASE_URL || "";
const supabaseAnonKey = process.env.SUPABASE_ANON_KEY || process.env.VITE_SUPABASE_ANON_KEY || "";
const supabase = (supabaseUrl && supabaseAnonKey) ? createClient(supabaseUrl, supabaseAnonKey) : null;

if (supabase) {
  console.log("[Supabase] Real Supabase client successfully initialized on backend server.");
} else {
  console.log("[Supabase] No credentials found. Using local JSON files database.");
}

interface Machine {
  id: string;
  code: string;
  name: string;
  status: 'EMPTY' | 'PLAN' | 'ALERT_ABSENT' | 'REPLACED';
  operator: string;
  updatedAt: string;
}

interface HistoryEvent {
  id: string;
  timestamp: string;
  dateString: string;
  machineId: string;
  machineName: string;
  status: 'EMPTY' | 'PLAN' | 'ALERT_ABSENT' | 'REPLACED';
  operator: string;
  code: string;
  source: 'WEB_MANUAL' | 'TELEGRAM' | 'SYSTEM_INIT';
}

interface PlanEntry {
  machineId: string;
  // Multi-shift fields (3-shift or 2-shift)
  shift1?: string; // Ca 1 (3-shift) or Ca Sáng (2-shift)
  shift2?: string; // Ca 2 (3-shift) or Ca Đêm (2-shift)
  shift3?: string; // Ca 3 (3-shift only)
}

interface ShiftPlan {
  plannedDate: string;
  shiftType: '3_shifts' | '2_shifts'; // Loại ca: 3 ca (8 giờ) hoặc 2 ca (12 giờ)
  note: string;
  entries: PlanEntry[];
  updatedAt: string;
}

// --- Global Database Memory Cache ---
let cacheMachines: Machine[] = [];
let cacheHistory: HistoryEvent[] = [];
let cachePlan: ShiftPlan = { plannedDate: "", shiftType: "3_shifts", note: "", entries: [], updatedAt: "" };
let cacheAdmins: AdminUser[] = [];

// --- Local JSON File Fallback Helpers ---
function readLocalPlan(): ShiftPlan {
  try {
    if (fs.existsSync(PLAN_FILE)) {
      const content = fs.readFileSync(PLAN_FILE, "utf-8");
      const parsed = JSON.parse(content);
      // Migrate old single-operator entries to multi-shift format
      if (parsed.entries && parsed.entries.length > 0 && typeof parsed.entries[0].operator === 'string') {
        parsed.entries = parsed.entries.map((e: any) => ({
          machineId: e.machineId,
          shift1: e.operator || '',
          shift2: '',
          shift3: ''
        }));
      }
      if (!parsed.shiftType) parsed.shiftType = '3_shifts';
      return parsed;
    }
  } catch (error) {
    console.error("Error reading local plan file:", error);
  }
  const tomorrow = new Date();
  tomorrow.setDate(tomorrow.getDate() + 1);
  const yyyy = tomorrow.getFullYear();
  const mm = String(tomorrow.getMonth() + 1).padStart(2, '0');
  const dd = String(tomorrow.getDate()).padStart(2, '0');
  return {
    plannedDate: `${yyyy}-${mm}-${dd}`,
    shiftType: '3_shifts',
    note: '',
    entries: [],
    updatedAt: new Date().toISOString()
  };
}

function writeLocalPlan(plan: ShiftPlan) {
  try {
    fs.writeFileSync(PLAN_FILE, JSON.stringify(plan, null, 2), "utf-8");
  } catch (error) {
    console.error("Error writing local plan file:", error);
  }
}

function readLocalHistory(): HistoryEvent[] {
  try {
    if (fs.existsSync(HISTORY_FILE)) {
      const content = fs.readFileSync(HISTORY_FILE, "utf-8");
      return JSON.parse(content);
    }
  } catch (error) {
    console.error("Error reading local history file:", error);
  }
  
  // Default history
  const defaultHistory: HistoryEvent[] = [
    {
      id: "evt-init-1",
      timestamp: "2026-05-28T01:00:23.000Z",
      dateString: "2026-05-28",
      machineId: "do_kim_loai_mam_rung",
      machineName: "Máy dò kim loại - Mâm rung",
      status: "PLAN",
      operator: "VN006666",
      code: "VN006666",
      source: "TELEGRAM"
    },
    {
      id: "evt-init-2",
      timestamp: "2026-05-28T02:15:10.000Z",
      dateString: "2026-05-28",
      machineId: "mdg_1",
      machineName: "MĐG 1",
      status: "PLAN",
      operator: "VN002759",
      code: "VN002759",
      source: "TELEGRAM"
    },
    {
      id: "evt-init-3",
      timestamp: "2026-05-28T04:30:45.000Z",
      dateString: "2026-05-28",
      machineId: "mdg_1",
      machineName: "MĐG 1",
      status: "ALERT_ABSENT",
      operator: "VN002759",
      code: "VN002759",
      source: "TELEGRAM"
    },
    {
      id: "evt-init-4",
      timestamp: "2026-05-28T04:45:00.000Z",
      dateString: "2026-05-28",
      machineId: "mdg_1",
      machineName: "MĐG 1",
      status: "REPLACED",
      operator: "VN003309",
      code: "VN003309",
      source: "WEB_MANUAL"
    },
    {
      id: "evt-init-5",
      timestamp: "2026-05-28T07:22:18.000Z",
      dateString: "2026-05-28",
      machineId: "ep_mieng_3",
      machineName: "Ép miệng 3",
      status: "PLAN",
      operator: "VN008888",
      code: "VN008888",
      source: "WEB_MANUAL"
    },
    {
      id: "evt-init-6",
      timestamp: "2026-05-29T00:30:15.000Z",
      dateString: "2026-05-29",
      machineId: "bang_tai_dai_3",
      machineName: "Băng tải dài 3",
      status: "PLAN",
      operator: "VN009999",
      code: "VN009999",
      source: "TELEGRAM"
    },
    {
      id: "evt-init-7",
      timestamp: "2026-05-29T01:12:00.000Z",
      dateString: "2026-05-29",
      machineId: "ep_mieng_1",
      machineName: "Ép miệng 1",
      status: "PLAN",
      operator: "VN002759",
      code: "VN002759",
      source: "WEB_MANUAL"
    },
    {
      id: "evt-init-8",
      timestamp: "2026-05-29T02:05:40.000Z",
      dateString: "2026-05-29",
      machineId: "can",
      machineName: "Cân",
      status: "PLAN",
      operator: "VN007777",
      code: "VN007777",
      source: "TELEGRAM"
    }
  ];
  writeLocalHistory(defaultHistory);
  return defaultHistory;
}

function writeLocalHistory(history: HistoryEvent[]) {
  try {
    fs.writeFileSync(HISTORY_FILE, JSON.stringify(history, null, 2), "utf-8");
  } catch (error) {
    console.error("Error writing local history file:", error);
  }
}

// --- Active Cache Sync / Supabase Sync Function ---
async function initializeDatabase() {
  // Load local backups first (immediate local load)
  cacheMachines = readLocalMachines();
  cacheHistory = readLocalHistory();
  cachePlan = readLocalPlan();
  cacheAdmins = readLocalAdmins();

  if (!supabase) {
    console.log("[Database] Loaded local backup files. Running in Local Mode.");
    return;
  }

  console.log("[Database] Supabase credentials detected. Synchronizing cloud database...");
  try {
    // 1. Sync Machines
    const { data: dbMachines } = await supabase.from("machines").select("*");
    if (dbMachines && dbMachines.length > 0) {
      cacheMachines = dbMachines as Machine[];
      writeLocalMachines(cacheMachines);
      console.log(`[Supabase] Synced ${cacheMachines.length} machines from cloud.`);
    } else {
      // Seed Cloud with default local database
      await supabase.from("machines").upsert(cacheMachines);
      console.log("[Supabase] Seeded cloud database with initial machine positions.");
    }

    // 2. Sync History
    const { data: dbHistory } = await supabase.from("history").select("*");
    if (dbHistory && dbHistory.length > 0) {
      cacheHistory = dbHistory as HistoryEvent[];
      writeLocalHistory(cacheHistory);
      console.log(`[Supabase] Synced ${cacheHistory.length} history events from cloud.`);
    } else if (cacheHistory.length > 0) {
      await supabase.from("history").upsert(cacheHistory);
    }

    // 3. Sync Plan
    const { data: dbPlans } = await supabase.from("plans").select("*");
    if (dbPlans && dbPlans.length > 0) {
      const sorted = (dbPlans as ShiftPlan[]).sort((a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime());
      cachePlan = sorted[0];
      writeLocalPlan(cachePlan);
      console.log(`[Supabase] Synced shift plan for date ${cachePlan.plannedDate} from cloud.`);
    } else {
      await supabase.from("plans").upsert({
        plannedDate: cachePlan.plannedDate,
        shiftType: cachePlan.shiftType,
        note: cachePlan.note,
        entries: cachePlan.entries,
        updatedAt: cachePlan.updatedAt
      });
    }

    // 4. Sync Admins
    const { data: dbAdmins } = await supabase.from("admins").select("*");
    if (dbAdmins && dbAdmins.length > 0) {
      cacheAdmins = dbAdmins as AdminUser[];
      writeLocalAdmins(cacheAdmins);
      console.log(`[Supabase] Synced ${cacheAdmins.length} admin accounts from cloud.`);
    } else if (cacheAdmins.length > 0) {
      await supabase.from("admins").upsert(cacheAdmins);
    }

    console.log("[Database] Supabase synchronization successfully completed.");

    // Start background poller every 5 seconds to keep database updated across multiple servers/instances
    setInterval(async () => {
      try {
        const { data: remoteMachines } = await supabase.from("machines").select("*");
        if (remoteMachines && remoteMachines.length > 0) {
          cacheMachines = remoteMachines as Machine[];
          writeLocalMachines(cacheMachines);
        }

        const { data: remotePlans } = await supabase.from("plans").select("*");
        if (remotePlans && remotePlans.length > 0) {
          const sorted = (remotePlans as ShiftPlan[]).sort((a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime());
          if (sorted[0] && sorted[0].updatedAt !== cachePlan.updatedAt) {
            cachePlan = sorted[0];
            writeLocalPlan(cachePlan);
          }
        }
      } catch (err) {
        console.error("[Supabase Background Sync Warning]:", err);
      }
    }, 5000);

  } catch (err) {
    console.error("[Supabase Initial Sync Failure, falling back to local files]:", err);
  }
}

// --- Synchronous Accessors mapped to synced in-memory cache ---
function readPlan(): ShiftPlan {
  return cachePlan;
}

function writePlan(plan: ShiftPlan) {
  cachePlan = plan;
  writeLocalPlan(plan);
  if (supabase) {
    supabase.from("plans").upsert({
      plannedDate: plan.plannedDate,
      shiftType: plan.shiftType,
      note: plan.note,
      entries: plan.entries,
      updatedAt: plan.updatedAt
    }).then(({ error }) => {
      if (error) console.error("[Supabase Async Write Error (plans)]:", error);
    });
  }
}

function readHistory(): HistoryEvent[] {
  return cacheHistory;
}

function writeHistory(history: HistoryEvent[]) {
  cacheHistory = history;
  writeLocalHistory(history);
  if (supabase) {
    supabase.from("history").upsert(history).then(({ error }) => {
      if (error) console.error("[Supabase Async Write Error (history)]:", error);
    });
  }
}

function logEvent(machine: Machine, source: 'WEB_MANUAL' | 'TELEGRAM' | 'SYSTEM_INIT') {
  try {
    const history = readHistory();
    const now = new Date();
    const yyyy = now.getFullYear();
    const mm = String(now.getMonth() + 1).padStart(2, '0');
    const dd = String(now.getDate()).padStart(2, '0');
    const dateString = `${yyyy}-${mm}-${dd}`;
    
    const newEvent: HistoryEvent = {
      id: `evt-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
      timestamp: now.toISOString(),
      dateString,
      machineId: machine.id,
      machineName: machine.name,
      status: machine.status,
      operator: machine.operator,
      code: machine.code || machine.operator,
      source
    };
    
    history.push(newEvent);
    writeHistory(history);
  } catch (e) {
    console.error("Error logging event:", e);
  }
}

const DEFAULT_MACHINES: Machine[] = [
  // 1. Máy Dò Kim Loại - Mâm Rung
  {
    id: "do_kim_loai_mam_rung",
    code: "",
    name: "Máy dò kim loại - Mâm rung",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  // 2. Băng tải dài 1-4
  {
    id: "bang_tai_dai_1",
    code: "",
    name: "Băng tải dài 1",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "bang_tai_dai_2",
    code: "",
    name: "Băng tải dài 2",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "bang_tai_dai_3",
    code: "",
    name: "Băng tải dài 3",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "bang_tai_dai_4",
    code: "",
    name: "Băng tải dài 4",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  // 3. MĐG 1 to 7
  {
    id: "mdg_1",
    code: "",
    name: "MĐG 1",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_2",
    code: "",
    name: "MĐG 2",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_3",
    code: "",
    name: "MĐG 3",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_4",
    code: "",
    name: "MĐG 4",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_5",
    code: "",
    name: "MĐG 5",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_6",
    code: "",
    name: "MĐG 6",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "mdg_7",
    code: "",
    name: "MĐG 7",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  // 4. Bóc túi 1 to 3
  {
    id: "boc_tui_1",
    code: "",
    name: "Bóc túi 1",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "boc_tui_2",
    code: "",
    name: "Bóc túi 2",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "boc_tui_3",
    code: "",
    name: "Bóc túi 3",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  // 5. Cân
  {
    id: "can",
    code: "",
    name: "Cân",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  // 6. Ép Miệng 1 to 7
  {
    id: "ep_mieng_1",
    code: "",
    name: "Ép miệng 1",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_2",
    code: "",
    name: "Ép miệng 2",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_3",
    code: "",
    name: "Ép miệng 3",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_4",
    code: "",
    name: "Ép miệng 4",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_5",
    code: "",
    name: "Ép miệng 5",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_6",
    code: "",
    name: "Ép miệng 6",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  },
  {
    id: "ep_mieng_7",
    code: "",
    name: "Ép miệng 7",
    status: "EMPTY",
    operator: "",
    updatedAt: new Date().toISOString()
  }
];

function readLocalMachines(): Machine[] {
  try {
    if (fs.existsSync(DB_FILE)) {
      const content = fs.readFileSync(DB_FILE, "utf-8");
      const data = JSON.parse(content);
      if (Array.isArray(data) && data.length === DEFAULT_MACHINES.length) {
        return data;
      }
    }
  } catch (error) {
    console.error("Error reading local database file:", error);
  }
  writeLocalMachines(DEFAULT_MACHINES);
  return DEFAULT_MACHINES;
}

function writeLocalMachines(data: Machine[]) {
  try {
    fs.writeFileSync(DB_FILE, JSON.stringify(data, null, 2), "utf-8");
  } catch (error) {
    console.error("Error writing local database file:", error);
  }
}

function readDB(): Machine[] {
  if (cacheMachines.length === 0) {
    cacheMachines = readLocalMachines();
  }
  return cacheMachines;
}

function writeDB(data: Machine[]) {
  cacheMachines = data;
  writeLocalMachines(data);
  if (supabase) {
    supabase.from("machines").upsert(data).then(({ error }) => {
      if (error) console.error("[Supabase Async Write Error (machines)]:", error);
    });
  }
}

// Normalize Vietnamese helper to search diacritic-agnostic labels
function normalizeVietnamese(str: string): string {
  return str.toLowerCase()
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/đ/g, "d")
    .trim();
}

const ADMINS_FILE = path.join(process.cwd(), "admins-db.json");

interface AdminUser {
  username: string;
  passwordHash: string;
  createdAt: string;
  approved: boolean;
  approvedBy?: string;
}

function readLocalAdmins(): AdminUser[] {
  try {
    if (fs.existsSync(ADMINS_FILE)) {
      const content = fs.readFileSync(ADMINS_FILE, "utf-8");
      return JSON.parse(content);
    }
  } catch (error) {
    console.error("Error reading local admins database:", error);
  }
  return [];
}

function writeLocalAdmins(admins: AdminUser[]) {
  try {
    fs.writeFileSync(ADMINS_FILE, JSON.stringify(admins, null, 2), "utf-8");
  } catch (error) {
    console.error("Error writing local admins database:", error);
  }
}

function readAdmins(): AdminUser[] {
  if (cacheAdmins.length === 0) {
    cacheAdmins = readLocalAdmins();
  }
  return cacheAdmins;
}

function writeAdmins(admins: AdminUser[]) {
  cacheAdmins = admins;
  writeLocalAdmins(admins);
  if (supabase) {
    supabase.from("admins").upsert(admins).then(({ error }) => {
      if (error) console.error("[Supabase Async Write Error (admins)]:", error);
    });
  }
}

// Auto-initialize default admin if none exist
try {
  let existingAdmins = readAdmins();
  if (existingAdmins.length === 0) {
    const defaultHash = crypto.createHash("sha256").update("admin123").digest("hex");
    writeAdmins([{
      username: "admin",
      passwordHash: defaultHash,
      createdAt: new Date().toISOString(),
      approved: true,
      approvedBy: "SYSTEM"
    }]);
    console.log("[Auth] Default admin account successfully created (username: 'admin', password: 'admin123')");
  } else {
    // Migrate existing accounts to have approved check
    let changed = false;
    const migrated = existingAdmins.map((adm) => {
      if (adm.approved === undefined) {
        adm.approved = true; // Auto-approve pre-existing accounts
        changed = true;
      }
      return adm;
    });
    if (changed) {
      writeAdmins(migrated);
    }
  }
} catch (e) {
  console.error("Error initializing default admin:", e);
}

async function startServer() {
  await initializeDatabase();
  const app = express();
  const PORT = Number(process.env.PORT) || 3000;

  app.use(express.json());

  // Admin authentication APIs
  app.post("/api/admin/register", (req, res) => {
    try {
      const { username, password } = req.body;
      if (!username || !password) {
        return res.status(400).json({ error: "Vui lòng nhập đầy đủ tên tài khoản và mật khẩu!" });
      }
      if (username.length < 3) {
        return res.status(400).json({ error: "Tên tài khoản phải từ 3 ký tự trở lên!" });
      }
      if (password.length < 4) {
        return res.status(400).json({ error: "Mật khẩu phải từ 4 ký tự trở lên!" });
      }

      const adminsList = readAdmins();
      const exists = adminsList.some(admin => admin.username.toLowerCase() === username.toLowerCase());
      if (exists) {
        return res.status(400).json({ error: "Tên tài khoản này đã được sử dụng trên hệ thống!" });
      }

      const passwordHash = crypto.createHash("sha256").update(password).digest("hex");
      adminsList.push({
        username,
        passwordHash,
        createdAt: new Date().toISOString(),
        approved: false // New admin registration must be approved by an active admin!
      });
      writeAdmins(adminsList);

      return res.json({ 
        success: true, 
        message: "Yêu cầu đăng ký tài khoản quản trị viên đã gửi thành công! Hãy liên hệ tài khoản Admin cấp cao phê duyệt kích hoạt." 
      });
    } catch (err: any) {
      console.error("Registration error:", err);
      return res.status(500).json({ error: "Lỗi máy chủ khi xử lý đăng ký!" });
    }
  });

  app.post("/api/admin/login", (req, res) => {
    try {
      const { username, password } = req.body;
      if (!username || !password) {
        return res.status(400).json({ error: "Vui lòng nhập đầy đủ tên tài khoản và mật khẩu!" });
      }

      const adminsList = readAdmins();
      const targetHash = crypto.createHash("sha256").update(password).digest("hex");
      const match = adminsList.find(admin => admin.username.toLowerCase() === username.toLowerCase() && admin.passwordHash === targetHash);

      if (match) {
        if (!match.approved) {
          return res.status(403).json({ 
            error: "Tài khoản chưa được kích hoạt! Hãy liên hệ người quản trị hiện tại để phê duyệt tài khoản này." 
          });
        }
        return res.json({ 
          success: true, 
          message: "Đăng nhập thành công!", 
          admin: { username: match.username } 
        });
      }

      return res.status(401).json({ error: "Tài khoản hoặc mật khẩu không chính xác!" });
    } catch (err: any) {
      console.error("Login error:", err);
      return res.status(500).json({ error: "Lỗi máy chủ khi xử lý đăng nhập!" });
    }
  });

  // Admin list / approve / delete APIs
  app.get("/api/admin/list", (req, res) => {
    try {
      const { requester } = req.query;
      if (!requester) {
        return res.status(400).json({ error: "Thiếu thông tin người phê duyệt!" });
      }
      const adminsList = readAdmins();
      const isApprovedRequester = adminsList.some(admin => admin.username.toLowerCase() === String(requester).toLowerCase() && admin.approved === true);
      if (!isApprovedRequester) {
        return res.status(403).json({ error: "Bạn không có quyền thực hiện chức năng này!" });
      }

      // Return lists with password hashes stripped out
      const sanitized = adminsList.map(a => ({
        username: a.username,
        createdAt: a.createdAt,
        approved: a.approved,
        approvedBy: a.approvedBy
      }));
      return res.json(sanitized);
    } catch (err) {
      console.error("fetch list error:", err);
      return res.status(500).json({ error: "Lỗi máy chủ!" });
    }
  });

  app.post("/api/admin/approve", (req, res) => {
    try {
      const { requester, target } = req.body;
      if (!requester || !target) {
        return res.status(400).json({ error: "Thiếu trường dữ liệu phê kích hoạt!" });
      }
      const adminsList = readAdmins();
      const isApprovedRequester = adminsList.some(admin => admin.username.toLowerCase() === String(requester).toLowerCase() && admin.approved === true);
      if (!isApprovedRequester) {
        return res.status(403).json({ error: "Chỉ tài khoản admin đã kích hoạt mới có quyền phê duyệt!" });
      }

      const targetAdmin = adminsList.find(admin => admin.username.toLowerCase() === String(target).toLowerCase());
      if (!targetAdmin) {
        return res.status(404).json({ error: "Không tìm thấy tài khoản để phê duyệt!" });
      }

      targetAdmin.approved = true;
      targetAdmin.approvedBy = String(requester);
      writeAdmins(adminsList);

      return res.json({ success: true, message: `Kích hoạt thành công tài khoản quản trị của ${targetAdmin.username}!` });
    } catch (err) {
      console.error("approve error:", err);
      return res.status(500).json({ error: "Lỗi máy chủ khi phê duyệt!" });
    }
  });

  app.post("/api/admin/delete", (req, res) => {
    try {
      const { requester, target } = req.body;
      if (!requester || !target) {
        return res.status(400).json({ error: "Thiếu thông tin người yêu cầu xóa!" });
      }
      if (String(target).toLowerCase() === "admin") {
        return res.status(400).json({ error: "Không thể xóa tài khoản Quản trị viên tối cao mặc định!" });
      }
      if (String(target).toLowerCase() === String(requester).toLowerCase()) {
        return res.status(400).json({ error: "Bạn không được tự xóa tài khoản của chính mình!" });
      }

      const adminsList = readAdmins();
      const isApprovedRequester = adminsList.some(admin => admin.username.toLowerCase() === String(requester).toLowerCase() && admin.approved === true);
      if (!isApprovedRequester) {
        return res.status(403).json({ error: "Chỉ tài khoản admin đã kích hoạt mới được xóa!" });
      }

      const updated = adminsList.filter(admin => admin.username.toLowerCase() !== String(target).toLowerCase());
      if (updated.length === adminsList.length) {
        return res.status(404).json({ error: "Không tìm thấy tài khoản!" });
      }

      writeAdmins(updated);
      return res.json({ success: true, message: `Đã loại bỏ tài khoản ${target} khỏi hệ thống.` });
    } catch (err) {
      console.error("delete error:", err);
      return res.status(500).json({ error: "Lỗi máy chủ khi xử lý!" });
    }
  });

  // API Endpoints
  
  // 1. Fetch current status of 4 machines
  app.get("/api/machines", (req, res) => {
    const list = readDB();
    res.json(list);
  });

  // 1.5. Query history records by date (for search and PDF extraction)
  app.get("/api/history", (req, res) => {
    const { date } = req.query;
    const history = readHistory();
    if (date) {
      const filtered = history.filter(evt => evt.dateString === date);
      return res.json(filtered);
    }
    return res.json(history);
  });

  // 2. Submit status overrides from the admin panel (Manual configuration override)
  app.post("/api/machines", (req, res) => {
    const updatedModel = req.body;
    if (Array.isArray(updatedModel)) {
      writeDB(updatedModel);
      return res.json({ success: true, count: updatedModel.length });
    }
    
    const { id, status, operator, code } = updatedModel;
    const current = readDB();
    const index = current.findIndex(m => m.id === id);
    if (index !== -1) {
      const valStatus = status || current[index].status;
      const valOperator = operator !== undefined ? operator : current[index].operator;
      const valCode = code !== undefined ? code : (operator !== undefined ? operator : current[index].code);

      current[index] = {
        ...current[index],
        status: valStatus,
        operator: valOperator,
        code: valCode,
        updatedAt: new Date().toISOString()
      };
      writeDB(current);
      logEvent(current[index], 'WEB_MANUAL');

      return res.json({ success: true, updated: current[index] });
    }
    res.status(404).json({ error: "Machine not found" });
  });

  // 3. Shift Plan APIs
  app.get("/api/plan", (req, res) => {
    res.json(readPlan());
  });

  app.post("/api/plan", (req, res) => {
    try {
      const { plannedDate, shiftType, note, entries } = req.body;
      if (!plannedDate || !Array.isArray(entries)) {
        return res.status(400).json({ error: "Invalid plan payload" });
      }
      const plan: ShiftPlan = {
        plannedDate,
        shiftType: shiftType || '3_shifts',
        note: note || '',
        entries,
        updatedAt: new Date().toISOString()
      };
      writePlan(plan);
      return res.json({ success: true, plan });
    } catch (err) {
      console.error("Error saving plan:", err);
      return res.status(500).json({ error: "Server error saving plan" });
    }
  });

  // Activate: copy selected shift's operators into live machines-db (sets status PLAN)
  app.post("/api/plan/activate", (req, res) => {
    try {
      const { shift } = req.body; // shift = 'shift1' | 'shift2' | 'shift3'
      const targetShift: 'shift1' | 'shift2' | 'shift3' = (['shift1', 'shift2', 'shift3'].includes(shift)) ? shift : 'shift1';

      const plan = readPlan();
      const current = readDB();
      const now = new Date().toISOString();

      let activatedCount = 0;

      const updated = current.map(machine => {
        const entry = plan.entries.find(e => e.machineId === machine.id);
        const operatorCode = (entry ? (entry[targetShift] || '') : '').trim();

        if (operatorCode) {
          activatedCount++;
          const m = {
            ...machine,
            operator: operatorCode,
            code: operatorCode,
            status: 'PLAN' as const,
            updatedAt: now
          };
          logEvent(m, 'WEB_MANUAL');
          return m;
        } else {
          // Clear positions not assigned in this shift
          return {
            ...machine,
            operator: '',
            code: '',
            status: 'EMPTY' as const,
            updatedAt: now
          };
        }
      });

      writeDB(updated);

      return res.json({ success: true, activatedCount, shift: targetShift });
    } catch (err) {
      console.error("Error activating plan:", err);
      return res.status(500).json({ error: "Server error activating plan" });
    }
  });



  // Serve Single Page Application (SPA) inside AI Studio
  if (process.env.NODE_ENV !== "production") {
    const vite = await createViteServer({
      server: { 
        middlewareMode: true,
        hmr: false 
      },
      appType: "spa",
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(process.cwd(), "dist");
    app.use(express.static(distPath));
    app.get("*", (req, res) => {
      res.sendFile(path.join(distPath, "index.html"));
    });
  }

  app.listen(PORT, "0.0.0.0", () => {
    console.log(`Factory server starting on port ${PORT}`);
  });
}

startServer().catch(console.error);
