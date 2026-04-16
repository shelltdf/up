# `package.xml` 涓?`target.xml` 瑙勮寖璇存槑

鏈枃妗ｆ弿杩?**up** 褰撳墠瀹炵幇瀵逛袱绉嶆弿杩版枃浠剁殑**瑙ｆ瀽绾﹀畾**涓?**configure** 闃舵鐨?*璇箟绾︽潫**銆傚疄鐜伴噰鐢ㄨ交閲忔鍒欐壂鎻忥紙瑙?`src/simple_xml.cpp`锛夛紝**涓嶆槸**瀹屾暣 XML 鏍￠獙鍣細寤鸿浠嶅啓鎴愯壇鏋?XML锛屽苟閬靛畧涓嬪垪鍙瘑鍒舰鎬併€?
---

## 1. 鏂囦欢瑙掕壊涓庢斁缃?
| 鏂囦欢 | 鏀剧疆浣嶇疆 | 浣滅敤 |
|------|----------|------|
| **`package.xml`** | 姣忎釜**鐙珛鍖?*鐨勬牴鐩綍锛堜笌鍖呭唴 `target.xml` 鏍戠殑涓婂眰涓€鑷达級 | 澹版槑鍖呭悕銆佺増鏈€?*鍖呯骇**渚濊禆锛堝叾浠栧寘鍚嶏級銆?|
| **`target.xml`** | 鍖呭唴**姣忎釜鏋勫缓鐩爣鐙崰涓€涓瓙鐩綍**锛岃鐩綍涓?*鎭板ソ涓€涓?* `target.xml` | 澹版槑鐩爣鍚嶃€佺被鍨嬨€佹簮鏂囦欢銆佸彲閫夌殑澶存枃浠舵悳绱㈣矾寰勩€?*鐩爣绾?*渚濊禆锛堝叾浠?target锛岄€氬父涓哄簱锛夈€?|

- **褰掑睘**锛歚target.xml` 蹇呴』浣嶄簬鏌愪竴 `package.xml` 鎵€鍦ㄧ洰褰曠殑**瀛愭爲鍐?*锛沗configure` 浼氭妸 target 褰掑埌璺緞涓?*鏈€杩?*鐨勫寘鏍逛笅锛堣 `configure.cpp` 涓?`nearest_package_parent`锛夈€?- **鎵弿**锛歚up configure` 鍦ㄦ壂鎻忔牴锛堥粯璁?cwd锛屾垨 `--scan` 鎸囧畾鐩綍锛変笅**閫掑綊**鏌ユ壘鎵€鏈?`package.xml` 涓?`target.xml`銆?
---

## 2. `package.xml`

### 2.1 鏍瑰厓绱?
- 鏂囦欢涓』鍑虹幇**绗竴涓?*鍖归厤瀛愪覆 **`<package`** 鐨勭墖娈碉紝瑙ｆ瀽鍣ㄥ彇鍏跺埌**绗竴涓?`>`** 涓烘鐨?*璧峰鏍囩**浣滀负鍖呭ご锛堜笉瑕佹眰瀹屾暣 XML 鏍戞牎楠岋級銆?- **蹇呴€夊睘鎬?*
  - **`name`**锛堝瓧绗︿覆锛夛細鍖呭悕銆傚湪涓€娆℃壂鎻忓唴蹇呴』**鍏ㄥ眬鍞竴**锛涗笌 `target.xml` 閲?`package:target` 褰㈠紡寮曠敤鏃剁殑鍖呭悕涓€鑷淬€?- **鍙€夊睘鎬?*
  - **`version`**锛堝瓧绗︿覆锛夛細鐗堟湰鍙枫€傜渷鐣ユ椂瀹炵幇涓婇粯璁や负 **`0.0.0`**銆?
绀轰緥锛?
```xml
<?xml version="1.0" encoding="UTF-8"?>
<package name="hello_demo" version="0.1.0">
  <dependency name="hello_simple_lib"/>
</package>
```

### 2.2 鍖呬緷璧?`<dependency ... />`

- 褰㈠紡锛氳嚜闂悎鏍囩 **`<dependency ... />`**锛屼笖鏍囩鍐呴渶鍖呭惈 **`name="..."`**锛堝弻寮曞彿瀛楃涓诧級銆?- **`name`**锛氭墍渚濊禆鐨?*鍏朵粬鍖?*鐨勫寘鍚嶏紱璇ュ寘蹇呴』鍦?*鍚屼竴娆?configure 鎵弿缁撴灉**涓嚭鐜帮紝鍚﹀垯锛?  - 鑻?**`optional="true"`**锛堟垨鍊间负 **`1` / `yes`**锛屽ぇ灏忓啓鎸夊疄鐜拌В鏋愶級锛氫粎浣滃彲閫夊０鏄庯紱
  - 鍚﹀垯 **configure 澶辫触**锛屾姤閿欐彁绀虹己灏戣鍖呫€?- **`optional`**锛氬彲閫夈€傝嫢瀛樺湪 **`optional="..."`**锛屼粎褰撳€间负 **`true` / `1` / `yes`** 鏃惰涓哄彲閫変緷璧栵紱鍏跺畠鍐欐硶瑙嗕负闈炲彲閫夈€?
瀹炵幇鐢ㄦ鍒欏尮閰嶆墍鏈?`<dependency` 鈥?`/>` 鐗囨锛?*涓?*瑕佹眰瀹冧滑宓屽湪鏌愪釜鐖惰妭鐐逛笅锛涗负鍙鎬у缓璁啓鍦?`<package>` 鍐呫€?
### 2.3 涓?`target.xml` 鐨勫叧绯?
鑻ユ湰鍖呭唴鏌?`target.xml` 浣跨敤 **`澶栧寘鍖呭悕:鐩爣鍚峘** 寮曠敤渚濊禆锛屽垯琚紩鐢ㄥ寘鍚?*蹇呴』**鍦ㄦ湰 `package.xml` 鐨?`<dependency name="璇ュ寘鍚?/>` 涓０鏄庯紙鍙€変緷璧栭櫎澶栨寜涓婅堪瑙勫垯锛夛紝鍚﹀垯 **configure 澶辫触**銆?
---

## 3. `target.xml`

### 3.1 鏍瑰厓绱?
- 椤诲瓨鍦?**`<target`** 璧峰鏍囩锛涜В鏋愭柟寮忎笌 `package` 鐩稿悓锛屽彇鍒扮涓€涓?**`>`** 涓烘銆?- **蹇呴€夊睘鎬?*
  - **`name`**锛欳Make 鐩爣鍚嶏紙涓庡彲鎵ц鏂囦欢鍚嶃€乣up run` 鎵€鐢ㄥ悕绛変竴鑷达級銆?- **鍙€夊睘鎬?*
  - **`type`**锛氱洰鏍囩被鍨嬨€傜渷鐣ユ椂榛樿涓?**`executable`**銆傚疄鐜拌瘑鍒紙澶у皬鍐欐寜鐢熸垚鍚庣浣跨敤鍓嶄负鍑嗭紝寤鸿灏忓啓锛夛細
    - **`executable`**锛氬彲鎵ц绋嬪簭銆?    - **`static_library`**锛氶潤鎬佸簱銆?    - **`shared_library`**锛氬姩鎬佸簱銆?
### 3.2 婧愭枃浠?`<file> ... </file>`

- 姣忎釜婧愭枃浠朵竴瀵规爣绛撅細**`<file>`** 涓?**`</file>`**锛堟爣绛惧悕鍓嶅悗鍏佽绌虹櫧锛屽 `</file >` 绛夊舰寮忛渶涓庡疄鐜版鍒欎竴鑷达紱**寤鸿**浣跨敤瑙勮寖鍐欐硶 `<file>...</file>`锛夈€?- 鏍囩鍐呮枃鏈紙鍘绘帀棣栧熬绌虹櫧锛変负**鐩稿璺緞**锛?*鐩稿浜庢湰 `target.xml` 鎵€鍦ㄧ洰褰?*銆?- 鍚屼竴鏂囦欢鍙嚭鐜?*澶氫釜** `<file>`锛岄『搴忓嵆鍔犲叆鏋勫缓鐨勬簮鏂囦欢鍒楄〃椤哄簭銆?
```xml
<sources>
  <file>main.cpp</file>
</sources>
```

璇存槑锛?*褰撳墠瑙ｆ瀽鍣ㄤ笉渚濊禆 `<sources>` 鐖惰妭鐐?*锛屽彧瑕佹枃浠朵腑瀛樺湪绗﹀悎妯″紡鐨?`<file>...</file>` 鍗充細鏀跺綍锛涗娇鐢?`<sources>` 浠呬负鍙鎬т笌涓?DESIGN 鍙欒堪涓€鑷淬€?
### 3.3 澶存枃浠惰緭鍏ヤ笌瀹夎杈撳嚭 `<includes>`

`<includes>` 缁熶竴浣跨敤 **`from/to`** 鑷棴鍚堟潯鐩紝鏀寔涓夌绫诲瀷锛?
- **`<dir from="..." to="..."/>`**
- **`<file from="..." to="..."/>`**
- **`<glob from="..." to="..."/>`**

鍏朵腑锛?
- **`from`**锛氬繀濉紝璺緞鐩稿 `target.xml` 鎵€鍦ㄧ洰褰曘€?- **`to`**锛氬彲閫夛紝瀹夎鐩爣鐩綍锛岀浉瀵瑰畨瑁呭墠缂€涓嬬殑 `include/`锛涚┖鍊兼垨鐪佺暐琛ㄧず瀹夎鍒?`include/` 鏍广€?
绀轰緥锛?
```xml
<includes>
  <dir from="../../include/rockBase" to="rockBase"/>
  <file from="../../include/common/version.hpp" to="common"/>
  <glob from="../../include/rockBase/*.hpp" to="rockBase"/>
</includes>
```

缂栬瘧鏈燂紙CMake 鐢熸垚锛夛細

- `dir.from` 鐩存帴浣滀负 include 鐩綍銆?- `file.from` 鍙栫埗鐩綍浣滀负 include 鐩綍銆?- `glob.from` 鍙?glob 鍩虹洰褰曚綔涓?include 鐩綍銆?
瀹夎鏈燂紙CMake 鐢熸垚锛夛細

- `dir` 鐢熸垚鐩綍瀹夎瑙勫垯锛歚install(DIRECTORY ... DESTINATION include/<to>)`銆?- `file` 鐢熸垚鏂囦欢瀹夎瑙勫垯锛歚install(FILES ... DESTINATION include/<to>)`銆?- `glob` 鍦?configure 闃舵瑙ｆ瀽鍖归厤鏂囦欢骞舵寜鏂囦欢瀹夎鍒?`include/<to>`锛堟棤鍖归厤浼氱粰 warning锛夈€?
> 鍏煎鎬ц鏄庯細鏃у啓娉?`<includes><dir>path</dir></includes>` 宸蹭笉鍐嶆敮鎸侊紱鑻ヤ粛浣跨敤浼氬湪 configure 闃舵鎶ラ敊銆?
### 3.4 鐩爣渚濊禆 `<dependency name="..."/>`

- 鑷棴鍚?**` <dependency name="..."/> `**锛屽彲澶氭鍑虹幇銆?- **`name`** 鏀寔涓ょ褰㈠紡锛?  1. **`鐩爣鍚峘**锛氫笌鏈寘鍐呮煇涓€**搴?* target 鍚屽悕锛岃〃绀轰緷璧栨湰鍖呰搴撱€?  2. **`鍖呭悕:鐩爣鍚峘**锛氫緷璧?*鍏朵粬鍖?*涓殑鏌愪竴搴?target锛堣鍖呴』鍦?`package.xml` 涓０鏄庯紝涓旇 target 椤诲湪鎵弿闆嗕腑瀛樺湪锛夈€?- **绾︽潫**锛氫緷璧栨寚鍚戠殑鐩爣绫诲瀷椤讳负 **`static_library` 鎴?`shared_library`**锛涜В鏋愪笉鍒版垨绫诲瀷涓嶅鍒?**configure 澶辫触**銆?
**鐢熸垚琛屼负锛圕Make 妯″紡锛屽綋鍓嶅疄鐜帮級**锛?
- **鍙墽琛岀洰鏍?*锛氶櫎鏄惧紡 `<dependency>` 澶栵紝浼?**PRIVATE 閾炬帴鏈寘鍐呭叏閮ㄥ簱 target**锛涘啀灏嗗悇 `<dependency>` 瑙ｆ瀽鍑虹殑搴撳悕骞跺叆閾炬帴鍒楄〃锛堝幓閲嶃€佹帓搴忥級銆?- **搴撶洰鏍?*锛歚<dependency>` 浼氬弬涓庝緷璧栨牎楠屼笌銆屽鍖呭寘鍐呭簱銆嶅姞鍏ョ敓鎴愬浘锛?*褰撳墠涓嶄細**涓洪潤鎬佸簱涓庨潤鎬佸簱涔嬮棿鐢熸垚 `target_link_libraries` 閾惧紡閾炬帴銆傝嫢鐢插簱瀹炵幇闇€璋冪敤涔欏簱绗﹀彿锛岄渶鍦ㄥ伐绋嬪眰闈㈣嚜琛屼繚璇侀摼鎺ラ『搴忔垨鍚堝苟鐩爣锛堜緥濡傜敱鏈€缁堝彲鎵ц鏂囦欢閾炬帴鍏ㄩ儴搴擄級锛涜瑙?`test_projects` 涓ず渚嬪彇鑸嶃€?
---

## 4. 缂栫爜涓庤矾寰?
- 鏂囦欢鍐呭寤鸿浣跨敤 **UTF-8**锛堜笌 [DESIGN.md](../DESIGN.md) 涓寘鎻忚堪缂栫爜璇存槑涓€鑷达級銆?- **configure** 浼氬鐩稿叧璺緞鍋?**ASCII 璺緞**绛夋牎楠岋紱璺緞涓惈闈?ASCII 绛夊彲鑳芥寜瀹炵幇鐩存帴鎶ラ敊锛岃閬垮厤銆?
---

## 5. configure 瀵广€屼富鍖呫€嶄笌鐩爣鐨勯澶栬姹?
- **涓诲寘**锛氫紭鍏堝彇 **cwd 绛変簬鍏?`package.xml` 鐖剁洰褰昤** 鐨勯偅涓€涓寘锛涜嫢鏃犲尮閰嶏紝鍒欏彇鎵弿鍒扮殑**绗竴涓?*鍖呬綔涓轰富鍖咃紙澶氬寘鍚屾壂鏃堕『搴忎緷璧栨枃浠剁郴缁燂紝寤鸿鍗曞寘鐩綍涓嬫墽琛屾垨鏄庣‘鏂囨。鍖栨壂鎻忛『搴忛闄╋級銆?- 涓诲寘涓嬮』鑷冲皯鏈?**涓€涓彲鎵ц** target锛屽惁鍒?configure 澶辫触銆?- 鍚屼竴 **`鍖呭悕:鐩爣鍚峘** 鍦ㄦ壂鎻忕粨鏋滀腑涓嶅緱閲嶅銆?
---

## 6. 娴嬭瘯涓庣ず渚?
浠撳簱 **`test_projects/`** 涓嬪悇瀛愮洰褰曚负瀹屾暣绀轰緥锛堝惈璺ㄥ寘渚濊禆銆佸搴撱€佸垎绂荤殑 `include/` / `src/` / `app/` / `test/` 绛夛級锛岃 [test_projects/README.md](../test_projects/README.md)銆?
---

## 7. 涓庡疄鐜扮殑瀵瑰簲鍏崇郴

| 璇濋 | 婧愮爜浣嶇疆 |
|------|----------|
| 瑙ｆ瀽 `package.xml` / `target.xml` | `src/simple_xml.cpp`锛坄load_package_xml` / `load_target_xml`锛?|
| 渚濊禆鏍￠獙銆佺敓鎴?CMake | `src/configure.cpp` |
| 鏁版嵁缁撴瀯 | `src/simple_xml.hpp`锛坄PackageDesc` / `TargetDesc`锛?|

鍚庣画鑻ュ紩鍏?XSD/JSON Schema锛屽彲鍦ㄦ湰鏂囦欢椤堕儴澧炲姞鐗堟湰鍙蜂笌鍙樻洿璁板綍銆?

---

## 8. assets and process stages (phase 1)

- `target.xml` supports `<assets>` with `dir/file/glob` entries.
- Each `sources` / `includes` / `assets` entry can optionally define:
  - `<preprocess command="..."/>`
  - `<postprocess command="..."/>`
- In phase 1, each stage accepts a single `command` only.
