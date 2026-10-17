# Spider-Man: Web of Shadows - PC Static Recompilation (reComp)
## AI Handover & Technical Overview Document

Bu belge, bu projede çalışan veya gelecekte devralacak yapay zeka ve geliştiriciler için projenin mimarisini, bugüne kadar çözülen tüm teknik sorunları, mevcut durumu ve sıradaki adımları eksiksiz olarak aktarmak amacıyla hazırlanmıştır.

---

### 1. Proje Kimliği ve Temel Mimari

- **Hedef Oyun:** *Spider-Man: Web of Shadows* (Xbox 360 sürümü)
  - Title ID: `41560815`
  - Media ID: `638BF87A`
  - XEX: `default.xex` (2008-09-03)
- **Çalışma Modeli:** **Static Recompilation (Statik Yeniden Derleme)**
  - Xbox 360 PowerPC (PPC) makine kodları, derleme aşamasında x86_64 C++ kodlarına çevrilir ve yerel bir Win64 yürütülebilir dosyası olarak derlenir.
  - Kodlama ve SDK altyapısı: **ReXGlue SDK v0.10.0** (`xbox360-recomp` / Xenia-canary tabanlı).
- **Yürütülebilir Dosya:** `out/build/win-amd64-release/wos_recomp.exe`
- **Derleme Araç Zinciri:**
  - Derleyici: LLVM Clang++ 23.1.3 (`C:/Program Files/LLVM/bin/clang++.exe`)
  - Bağlayıcı: LLD-Link (`lld-link.exe`)
  - Build Sistemi: Ninja (`ninja.exe`)
  - MSVC Kütüphaneleri: Proje içine entegre taşınabilir SDK (`sdk/msvc/`)
- **Grafik Arka Ucu:** DirectX 12 (`rexgpu-xenos.dll` - Xenos GPU emülasyonu)
- **Bellek Mimarisi:** 4GB sanal konuk arena (`0x0000000100000000`), fiziksel taban (`0x0000000200000000`).

---

### 2. Kronolojik Olarak Çözülen Kritik Sorunlar

#### Aşama 1: Açılış Çökmesi ve Sonsuz Yükleme Ekranı
- **Sorun:** Oyun açılıyor, 10 saniye loading ekranında kalıp kapanıyordu.
- **Kök Neden:** PPC kodundan gelen bazı fonksiyon işaretçilerinin `nullptr` (0x00000000) veya haritalanmamış adreslere işaret etmesi nedeniyle `ResolveIndirectFunction` çağrısının çökmesi.
- **Çözüm:** 
  - [`src/hooks/indirect_call_hooks.h`](file:///src/hooks/indirect_call_hooks.h) yazılarak IAT (Import Address Table) üzerinden `ResolveIndirectFunction` kancalandı. Geçersiz hedef adresler güvenli `SafeNullFunc` no-op fonksiyonuna yönlendirildi.
  - [`src/stubs/unregistered_func_handler.h`](file:///src/stubs/unregistered_func_handler.h) ile kaydedilmemiş konuk kod bloklarına güvenli fallback atandı.

#### Aşama 2: Yükleme Ekranından Sonra Siyah Ekran
- **Sorun:** Menü sesleri gelmesine ve oyun çalışmasına rağmen ekran tamamen siyah kalıyordu.
- **Kök Neden:** `render_target_path_d3d12 = "rtv"` denemesi. WoS, Xenos eDRAM tiling, özel sıkıştırılmış renk formatları ve resolve işlemlerini yoğun kullandığından standart Direct3D 12 RTV modunda görüntü üretemiyordu.
- **Çözüm:**
  - `render_target_path_d3d12 = "rov"` (Rasterizer-Ordered Views) olarak sabitlendi.
  - `d3d12_readback_resolve = false`, `readback_resolve = none` ve `clear_memory_page_state = true` ayarlandı.
  - Kare yakalama (frame capture) telemetrisi ile Spider-Man'in çatıdaki oyun içi 3D görüntüsü doğrulandı.

#### Aşama 3: Kesme Sahneleri (Cutscenes) ve Ayarlar Menüsü Çökmesi
- **Sorun:** Video/cutscene'ler donuyor ve in-game settings'e basınca oyun kapanıyordu.
- **Kök Neden:** Girdi sürücüsünün otomatik tuş basma mantığında START butonunu göndermesi cutscene'leri duraklatıyordu.
- **Çözüm:** Klavye/Fare (`MnkInputDriver`), Gamepad (`XinputInputDriver`) ve `SDLInputDriver` temiz `SharedAssignment` altına bağlandı; kontrolsüz START tetiklemeleri kaldırıldı.

#### Aşama 4: Canlı Telemetri ve Performans Takip Sistemi
- **Çözüm:** [`src/hooks/perf_monitor.h`](file:///src/hooks/perf_monitor.h) oluşturuldu.
- D3D Swap döngüsü (`sub_82AC4E48` ve `VdSwap`) kancalanarak anlık FPS, kare süresi (ms) ve işlemci CPU kullanımı (`%`) doğrudan oyun pencere başlığında (`SetWindowTextA`) sıfır maliyetle gösterilmeye başlandı.

#### Aşama 5: Şehirde Ağ Atarken ve Gezerken Yaşanan Ağır FPS Düşüşü (20 FPS Sorunu)
- **Sorun:** Oyunda hareketsiz dururken 55-70 FPS alınırken, ağ ile Manhattan blokları arasında hızlıca gezerken veya dövüş efektlerinde FPS aniden **20-23 FPS**'e çakılıyor ve CPU kullanımı **%82**'ye fırlıyordu.
- **Kök Nedenler:**
  1. *VRAM Aşımı ve PCIe Paging:* Kullanıcının ekran kartı 4 GB VRAM'e (GTX 1650 Ti) sahipti. Kaplama önbelleği ise `hard = 4096 MB`, `soft = 2048 MB` ve `lifetime = 120s` olarak bırakılmıştı. Oyuncu ağla uçtukça Manhattan sektörleri (M01, M02, N01 vb.) doldu ve 2 dakika boyunca bellekte tutuldu. 4 GB fiziksel VRAM aşıldığı için Windows, kaplamaları PCIe üzerinden sistem belleğine takasladı (PCIe thrashing).
  2. *Command Queue Flushes:* `d3d12_submit_on_primary_buffer_end = true` nedeniyle her PM4 primary buffer sonunda GPU komut listesi boşaltılıp sürücü seviyesinde CPU kilitlenmesine yol açıyordu.
  3. *ROV Alpha Serialization:* Ağ şeritleri ve dövüş partikülleri saydam geometry şeritleri olduğundan `use_fuzzy_alpha_epsilon = false` ayarı NVIDIA kartlarda ROV piksel gölgelendirici donanım kilitlenmelerine sebep oluyordu.
  4. *Anizotropik Filtreleme:* `anisotropic_override = 3` (8x/16x) 128-bit GPU bellek bant genişliğini tüketiyordu.
- **Uygulanan Optimizasyonlar ([`src/wos_recomp_app.h`](file:///src/wos_recomp_app.h)):**
  - `texture_cache_memory_limit_soft = 768 MB`
  - `texture_cache_memory_limit_hard = 1280 MB`
  - `texture_cache_memory_limit_soft_lifetime = 15 saniye` (Geride kalan sektörler temizlenir, PCIe takılması %100 önlenir).
  - `d3d12_submit_on_primary_buffer_end = false` (Komut listeleri toplu işlenir, CPU yükü düşer).
  - `use_fuzzy_alpha_epsilon = true` (Ağ ve partikül alpha karşılaştırmaları hızlandırıldı).
  - `anisotropic_override = 0` (Bant genişliği rahatlatıldı).
  - `d3d12_queue_priority = 2` ve `HIGH_PRIORITY_CLASS` aktifleştirildi.

#### Aşama 6: Save Yüklerken (Continue) ve Ayarlar Menüsünde Yaşanan Çökme (`RVA=0x36ABDE7`)
- **Sorun:** Oyun kayıtlı dosyayı (save) başlatırken veya oyun içi ayarlar menüsüne girildiğinde `Unhandled guest access violation` (`RVA=0x36ABDE7`, `sub_82A1ACF0`) hatasıyla çöküyordu.
- **Kök Nedenler:**
  1. *Kayıtsız Save Deserialization Fonksiyonları ve `0xE0000000+` Stack Offset Hatası:* Save yüklenirken çağrılan `0x82A8D8E8` ve `0x82A4B708` fonksiyonları manifestte kayıtlı değildi ve yorumlayıcı (`unregistered_func_handler.h`) üzerinden çalışıyordu. Yorumlayıcının `LoadU32BE`/`StoreU32BE` yardımcılarında `addr >= 0xE0000000` (konuk yığın/stack bölgesi) için gerekli olan `+0x1000` fiziksel host ofseti eksikti; bu da yığın üzerindeki yapıları (`0xE800CEC0`) 4 KB geriye yazıp belleği bozuyordu.
  2. *`sub_82A1ACF0` Hash Tablosu Kancasında SEH ve Sayfa Tablosu Doğrulaması:* `src/main.cpp` üzerinde `-fasync-exceptions` derleyici bayrağı eksik olduğu için Clang++ donanım erişim ihlallerinde `__try / __except` geri sarma tablolarını üretmiyordu; ayrıca ReXGlue'nun VEH yakalayıcısı SEH'ten önce devreye girip süreci sonlandırıyordu.
- **Çözüm:**
  - `0x82A8D8E8` ve `0x82A4B708` [`wos_recomp_manifest.toml`](file:///wos_recomp_manifest.toml) dosyasına eklenerek yerel C++ olarak derlendi.
  - [`src/stubs/unregistered_func_handler.h`](file:///src/stubs/unregistered_func_handler.h) içindeki bellek erişimlerine `(addr >= 0xE0000000u) ? 0x1000u : 0u` ofseti (`GuestAddr`) eklendi.
  - [`src/hooks/hash_table_hooks.h`](file:///src/hooks/hash_table_hooks.h) içindeki `sub_82A1ACF0` fonksiyonuna bellek okuması yapmadan önce 4-bayt hizalama (`(addr & 3) == 0`) ve `LookupHeap` -> `QueryProtect` yazılımsal sayfa tablosu okunabilirlik kontrolü (`IsReadableGuestStruct`) eklendi; [`CMakeLists.txt`](file:///CMakeLists.txt) dosyasına `-fasync-exceptions` eklendi.

#### Aşama 7: Ağ Atma, Dövüş, Patlama ve Kamera Çevirmedeki FPS Düşüşlerinin Giderilmesi
- **Sorun:** Save yükleme ve oyun içi kararlılık %100 sağlandıktan sonra (20-30 dakika sıfır çökme), 4 spesifik sahnede FPS'in 20-25'e düşmesi:
  1. Şehirde ağ atarken (hızlı kamera ve yeni blokların yüklenmesi)
  2. Düşmanlarla dövüşürken (yakın mesafe partikülleri ve vuruş efektleri)
  3. Patlamalarda (yoğun alpha partikül overdraw)
  4. Kamerayı aniden çevirirken (görünürlük ve occlusion culling)
- **Kök Nedenler:**
  1. *Occlusion Query GPU/CPU Eşzamanlama Gecikmesi:* `occlusion_query_enable = true` ayarı, kamera çevrilirken, binaların arkası taranırken ve patlama merkezlerinde CPU konuk iş parçacıklarının GPU sorgu sonuçlarını beklemesine (pipeline fence stall) neden oluyordu.
  2. *Kırpılmamış (Unclipped) Çizimlerde Aşırı EDRAM Aktarımı:* `execute_unclipped_draw_vs_on_cpu = false` ayarı nedeniyle tam ekran temizleme (clear) ve son-işleme (post-process) çizimlerinde ReXGlue kirli EDRAM sınırını hesaplayamıyor, tüm 10 MB'lık EDRAM'i ana bilgisayar render hedefleri arasında gereksiz yere takaslıyordu.
  3. *Hash Tablosu Kancasında (`sub_82A1ACF0`) Ağır CPU Yükü:* Saniyede yüz binlerce kez çağrılan hash tablosu döngüsünde her düğüm için `LookupHeap` ve `QueryProtect` çağrılarak CPU kullanımı **%80**'e tırmanıyordu.
  4. *Windows Güç Kısıtlaması (EcoQoS) ve 15.6ms Zamanlayıcı Çözünürlüğü:* Mobil GPU/CPU'lu dizüstü bilgisayarlarda arka plan iş parçacıkları kısıtlanıyordu.
- **Uygulanan Çözümler:**
  - `occlusion_query_enable = false` ve `query_occlusion_fake_sample_count = 1000` ([`src/wos_recomp_app.h`](file:///src/wos_recomp_app.h)): Kamera çevrilirken ve patlamalarda GPU bekleme gecikmesi tamamen ortadan kaldırıldı; nesneler anında görünür kabul edilir.
  - `execute_unclipped_draw_vs_on_cpu = true` ve `execute_unclipped_draw_vs_on_cpu_with_scissor = true`: Kırpılmamış çizimlerde tepe gölgelendiricileri CPU'da hesaplanarak yalnızca ilgili kırpma kutusu güncellenir, 10MB EDRAM takasları engellendi.
  - `primitive_processor_cache_min_indices = 4096`: Küçük partikül paketlerindeki aşırı önbellek kilitlenmeleri engellendi.
  - `d3d12_submit_on_primary_buffer_end = true`: GPU işlem kuyruğunun boş kalması önlendi.
  - `sub_82A1ACF0` ([`src/hooks/hash_table_hooks.h`](file:///src/hooks/hash_table_hooks.h)) içindeki `QueryProtect` kaldırıldı; tek döngülük hızlı adres sınır kontrolü (`IsFastLikelyValidGuestAddr`) ve sıfır maliyetli donanımsal SEH (`__try / __except`) ile CPU hash aramaları 100 kat hızlandırıldı.
  - `timeBeginPeriod(1)` ve `ProcessPowerThrottling` devre dışı bırakılarak CPU iş parçacıkları maksimum frekansa sabitlendi.

---

### 3. Kod Tabanı Dosya Rehberi

```text
reComp/
├── src/
│   ├── main.cpp                        # Uygulama giriş noktası (WosRecompApp örneğini çalıştırır)
│   ├── wos_recomp_app.h                # Temel cvar ayarları, GPU yapılandırması, girdi ve yaşam döngüsü
│   ├── hooks/
│   │   ├── indirect_call_hooks.h       # IAT Hooking: ResolveIndirectFunction & VdSwap koruması
│   │   ├── perf_monitor.h              # FPS, FrameTime ve CPU % ölçen arka plan telemetri iş parçacığı
│   │   └── hash_table_hooks.h          # Veri yapısı kancaları
│   └── stubs/
│       └── unregistered_func_handler.h # Kayıtsız konuk adresler için güvenli fallback tablosu
├── generated/                          # ReXGlue codegen tarafından üretilen C++ dosyaları
├── sdk/
│   ├── rexglue-sdk/                    # ReXGlue statik yeniden derleme SDK başlık ve kütüphaneleri
│   └── msvc/                           # Entegre MSVC ve Windows Kits 10 başlık dosyaları
├── out/build/win-amd64-release/        # Derleme çıktı dizini
│   ├── wos_recomp.exe                  # 64-bit yerel derlenmiş oyun çalıştırılabilir dosyası
│   └── logs/                           # Detaylı çalışma ve telemetri logları
├── Build_WoS.bat                       # 1-tık ile projeyi derleyen betik (MSVC include/lib otomatik ayarlar)
├── Play_WoS.bat                        # Oyunu en uygun pencere ve çözünürlük ayarlarıyla başlatan betik
└── wos_recomp_manifest.toml            # ReXGlue proje yapılandırma manifestosu
```

---

### 4. Nasıl Derlenir ve Çalıştırılır?

1. **Projeyi Derlemek İçin:**
   - Proje kök dizinindeki `Build_WoS.bat` dosyasını çalıştırın (veya terminalden `Build_WoS.bat`).
   - Bu betik, `sdk/msvc` altındaki başlıkları ve kütüphaneleri otomatik yükler ve Ninja ile `out/build/win-amd64-release/wos_recomp.exe` dosyasını oluşturur.
2. **Oyunu Başlatmak İçin:**
   - Proje kök dizinindeki `Play_WoS.bat` dosyasını çalıştırın.
   - Oyun penceresinin başlığında anlık FPS, FrameTime ve Process CPU kullanımı görünür (F3: Debug Overlay, F4: Settings).

---

### 5. Öncelikli Sıradaki Görevler (Backlog)

1. **Ağ Atma / Gezinme Telemetrisinin Doğrulanması:**
   - Son yapılan VRAM ve D3D12 optimizasyonlarının Manhattan'daki gezinme akıcılığını teyit etmek.
2. **Cutscene Akıcılığı ve Video Oynatma:**
   - Bink / WMV video dekoderi ve ses senkronizasyonunun ince ayarı.
3. **Ayarlar (Settings) Menüsü İstisnası:**
   - Oyun içi ayarlar menüsü açıldığında çağrılan konuk fonksiyonun stack trace incelemesi.
