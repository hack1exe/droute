<div align="center">
<img src="/external/vectors.png" width="105" height="104">
<h1>Droute</h1>
</div>

**Droute** is a lightweight utility that provides native proxy integration for Discord. 

It redirects all application traffic to a SOCKS proxy at the system level.

[<- Back to Russian](/README.md)

## 🤔 Why use Droute?
- **⚡ Autonomy:** Discord becomes a self-contained proxy client. It ignores system-wide VPN or proxy settings—if you turn off your main VPN, Discord will continue to operate through the specified proxy.
- **🤏 Zero-Config Routing:** Forget about messing with TUN interfaces, virtual network cards, or complex process-based filtering rules. The patch works inside the process, eliminating routing errors.
- **🕊️ Clean Network Stack:** The native hooking method doesn't create virtual adapters or clutter the Windows routing table. This prevents conflicts with anti-cheats, corporate VPNs, or other network filters.

## 🚀 How to use:
1. Download the latest version from the **[Releases](https://github.com/snowluwu/droute/releases)** tab.
2. **Launch Droute.**
3. Configure the proxy settings to fit your needs, then **save the changes.**
4. Click the **Install** button.
   - *The program will automatically locate Discord and install the patch into its directory.*
5. If you want to uninstall the patch, simply click the **Remove** button.

## 🔧 Technical Details
The project was built from scratch using C++ and C#, inspired by the [force-proxy](https://github.com/runetfreedom/force-proxy) implementation.
- **Integration:** Uses the [MinHook](https://github.com/tsudakageyu/minhook) library to intercept Windows API network calls (winsock2.h).
- **Patching:** The project utilizes the **Proxy DLL Side-loading** method, allowing code injection into the process address space in a simple and secure manner.

**The Patching Process:**
1. **Proxying:** The original system library `version.dll` is copied into the local Discord directory.
2. **Deployment:** The `droute.dll` module, containing the core traffic interception logic, is placed alongside it.
3. **PE-Patching (Import Table):** The Import Address Table (IAT) of the local `version.dll` copy is modified to include a recursive dependency on `droute.dll`.
4. **Execution:** Upon startup, the Windows Loader (LDR) finds the local `version.dll`, which automatically pulls `droute.dll` into the process memory.

This "Zero-Patching" approach allows for the secure integration of proxy features without modifying a single byte of the original Discord executable files.

## 👨‍💻 Development
To build the project yourself:
1. Clone the repository: `git clone --recursive https://github.com/snowluwu/droute.git`
2. Open the solution in **Visual Studio 2022**.
3. Build in **Release / Debug** mode.

## 🐛 Bugs and Suggestions
If something isn't working or you have ideas on how to improve **Droute**, feel free to open an [Issue!](https://github.com/snowluwu/droute/issues)

**Before creating a ticket, please:**
1. Ensure you are using the latest version of the patch.
2. Check if a similar issue has already been opened by other users.
3. Provide a detailed description and attach the `droute.log` file found in `%Temp%`.

**Your feedback helps make the project better for everyone! 🤝**
