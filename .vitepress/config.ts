import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
    title: "Component Model C++",
    description: " C++ ABI implementation of the WebAssembly Component Model",
    base: '/component-model-cpp/',
    srcExclude: ["build/**", "node_modules/**", "ref/**", "vcpkg/**", "vcpkg_overlays/**"],

    themeConfig: {
        editLink: {
            pattern: 'https://github.com/GordonSmith/component-model-cpp/edit/main/docs/:path',
            text: 'Edit this page on GitHub'
        },

        nav: [
            { text: 'Home', link: '/' },
            { text: 'Documentation', link: '/api-examples' },
            { text: 'GitHub', link: 'https://github.com/GordonSmith/component-model-cpp' },
            { text: 'Changelog', link: 'https://github.com/GordonSmith/component-model-cpp/blob/trunk/CHANGELOG.md' },
        ],

        sidebar: [

            {
                text: 'Documentation',
                items: [
                    { text: 'Generated Files Structure', link: '/docs/generated-files-structure' },
                    { text: 'Generated WAMR Helpers', link: '/docs/generated-wamr-helpers' },
                    { text: 'Packaging', link: '/docs/PACKAGING' }
                ]
            },
            {
                text: 'Examples & Guides',
                items: [
                    { text: 'Markdown Examples', link: '/markdown-examples' }
                ]
            },
            {
                text: 'Additional Resources',
                items: [
                    { text: 'Samples', link: '/samples/README' },
                    { text: 'Testing', link: '/test/README' },
                    { text: 'Grammar', link: '/grammar/README' },
                    { text: 'WIT Code Generator', link: '/tools/wit-codegen/README' }
                ]
            }
        ],

        socialLinks: [
            { icon: 'github', link: 'https://github.com/GordonSmith/component-model-cpp' },
            { icon: 'bluesky', link: 'https://bsky.app/profile/schmoo2k.bsky.social' },
            { icon: 'linkedin', link: 'https://www.linkedin.com/in/gordon-smith-0b168a6/' },
        ]
    }
})
