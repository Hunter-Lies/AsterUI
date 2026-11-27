import { defineConfig } from "vitepress";

const github = "https://github.com/Hunter-Lies/AsterUI";
const base = "/AsterUI/";

const zhNav = [
  { text: "指南", link: "/getting-started" },
  { text: "通讯", link: "/js-api" },
  { text: "架构", link: "/architecture" },
  { text: "GitHub", link: github }
];

const zhSidebar = [
  {
    text: "开始",
    items: [
      { text: "快速开始", link: "/getting-started" },
      { text: "完整 UI 教程", link: "/ui-guide" }
    ]
  },
  {
    text: "开发",
    items: [
      { text: "JavaScript 通讯", link: "/js-api" },
      { text: "预览与热重载", link: "/development" }
    ]
  },
  {
    text: "参考",
    items: [
      { text: "架构与边界", link: "/architecture" },
      { text: "故障排查", link: "/troubleshooting" },
      { text: "打包说明", link: "/packaging" }
    ]
  }
];

const enNav = [
  { text: "Guide", link: "/en/getting-started" },
  { text: "Bridge", link: "/en/js-api" },
  { text: "Architecture", link: "/en/architecture" },
  { text: "GitHub", link: github }
];

const enSidebar = [
  {
    text: "Start",
    items: [
      { text: "Getting started", link: "/en/getting-started" },
      { text: "Full UI guide", link: "/en/ui-guide" }
    ]
  },
  {
    text: "Develop",
    items: [
      { text: "JavaScript bridge", link: "/en/js-api" },
      { text: "Preview and HMR", link: "/en/development" }
    ]
  },
  {
    text: "Reference",
    items: [
      { text: "Architecture and scope", link: "/en/architecture" },
      { text: "Troubleshooting", link: "/en/troubleshooting" },
      { text: "Packaging", link: "/en/packaging" }
    ]
  }
];

export default defineConfig({
  lang: "zh-CN",
  title: "AsterUI",
  description: "Local HTML/CSS/JavaScript UI runtime for GTA V Legacy Edition",
  base,
  cleanUrls: true,
  lastUpdated: true,
  head: [
    ["meta", { name: "theme-color", content: "#6fae94" }],
    ["link", { rel: "icon", href: `${base}logo.svg` }]
  ],
  locales: {
    root: {
      label: "简体中文",
      lang: "zh-CN",
      title: "AsterUI",
      description: "GTA V 传承版本地 HTML UI 运行时",
      themeConfig: {
        nav: zhNav,
        sidebar: zhSidebar,
        outline: { level: [2, 3], label: "页面目录" },
        langMenuLabel: "切换语言",
        returnToTopLabel: "回到顶部",
        sidebarMenuLabel: "文档导航"
      }
    },
    en: {
      label: "English",
      lang: "en",
      link: "/en/",
      title: "AsterUI",
      description: "Local HTML UI runtime for GTA V Legacy Edition",
      themeConfig: {
        nav: enNav,
        sidebar: enSidebar,
        outline: { level: [2, 3], label: "On this page" },
        langMenuLabel: "Change language",
        returnToTopLabel: "Return to top",
        sidebarMenuLabel: "Documentation menu"
      }
    }
  },
  themeConfig: {
    logo: "/logo.svg",
    siteTitle: "AsterUI",
    search: {
      provider: "local"
    },
    socialLinks: [
      { icon: "github", link: github }
    ],
    editLink: {
      pattern: `${github}/edit/main/docs/:path`,
      text: "编辑此页"
    },
    footer: {
      message: "MIT licensed · UI runtime for local single-player overlays",
      copyright: "Copyright © 2026 AsterUI contributors"
    }
  }
});
