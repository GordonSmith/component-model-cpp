<script setup lang="ts">
import DefaultTheme from 'vitepress/theme'
import { useRoute } from 'vitepress'

const { Layout } = DefaultTheme
const route = useRoute()

// Don't show banner on home page since it has its own
const showBanner = route.path !== '/'
</script>

<template>
  <Layout>
    <template #doc-before>
      <div v-if="showBanner" class="construction-banner">
        <div class="custom-block warning">
          <p class="custom-block-title">⚠️ DOCUMENTATION UNDER CONSTRUCTION ⚠️</p>
          <p><strong>This documentation is being written from scratch.</strong> What you see here is essentially placeholder content and should not be trusted as accurate. Expect significant changes and updates.</p>
        </div>
      </div>
    </template>
  </Layout>
</template>

<style scoped>
.construction-banner {
  margin-bottom: 2rem;
}
</style>
