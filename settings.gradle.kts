pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

@Suppress("UnstableApiUsage")
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        // LatencyPlayer SDK - Nexus3 remote repo (anonymous read)
        maven {
            url = uri("http://192.168.3.160:8970/repository/maven-releases/")
            isAllowInsecureProtocol = true
        }
        // Local maven-repo (offline fallback when Nexus unreachable)
        maven {
            url = uri("${rootDir}/maven-repo")
            metadataSources {
                mavenPom()
                artifact()
            }
        }
    }
}

rootProject.name = "LatencyPlayerSDK"
include(":sdk")
include(":app")
