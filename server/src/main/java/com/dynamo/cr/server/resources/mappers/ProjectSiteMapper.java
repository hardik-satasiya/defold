package com.dynamo.cr.server.resources.mappers;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.clients.magazine.MagazineClient;
import com.dynamo.cr.server.model.*;

import java.util.List;
import java.util.Set;

public class ProjectSiteMapper {

    public static Protocol.ProjectSiteList map(User user, List<Project> projects, MagazineClient magazineClient) {
        Protocol.ProjectSiteList.Builder builder = Protocol.ProjectSiteList.newBuilder();

        for (Project project : projects) {
            builder.addSites(map(user, project, magazineClient));
        }

        return builder.build();
    }

    public static Protocol.ProjectSite map(User user, Project project, MagazineClient magazineClient) {
        Protocol.ProjectSite.Builder builder = Protocol.ProjectSite.newBuilder();

        builder.setProjectId(project.getId());

        if (user != null) {
            boolean isProjectMember = project.getMembers().stream().anyMatch(member -> member.getEmail().equals(user.getEmail()));
            builder.setIsAdmin(isProjectMember);
        } else {
            builder.setIsAdmin(false);
        }

        for (User member : project.getMembers()) {
            Protocol.UserInfo.Builder userInfoBuilder = Protocol.UserInfo.newBuilder();
            userInfoBuilder.setId(member.getId());
            userInfoBuilder.setEmail(""); // Don't expose user e-mail on public sites.
            userInfoBuilder.setFirstName(member.getFirstName());
            userInfoBuilder.setLastName(member.getLastName());
            builder.addMembers(userInfoBuilder.build());
        }

        if (project.getProjectSite() != null) {
            ProjectSite projectSite = project.getProjectSite();

            if (projectSite.isPlayableUploaded()) {
                builder.setPlayableUrl(magazineClient.createReadUrl("/projects/" + project.getId() + "/playable/index.html"));
            }

            if (projectSite.getPlayableImageUrl() != null) {
                builder.setPlayableImageUrl(magazineClient.createReadUrl(projectSite.getPlayableImageUrl()));
            }

            if (projectSite.getName() != null) {
                builder.setName(projectSite.getName());
            }

            if (projectSite.getDescription() != null) {
                builder.setDescription(projectSite.getDescription());
            }

            if (projectSite.getStudioUrl() != null) {
                builder.setStudioUrl(projectSite.getStudioUrl());
            }

            if (projectSite.getStudioName() != null) {
                builder.setStudioName(projectSite.getStudioName());
            }

            if (projectSite.getCoverImageUrl() != null) {
                builder.setCoverImageUrl(magazineClient.createReadUrl(projectSite.getCoverImageUrl()));
            }

            if (projectSite.getStoreFrontImageUrl() != null) {
                builder.setStoreFrontImageUrl(magazineClient.createReadUrl(projectSite.getStoreFrontImageUrl()));
            }

            if (projectSite.getDevLogUrl() != null) {
                builder.setDevLogUrl(projectSite.getDevLogUrl());
            }

            if (projectSite.getReviewUrl() != null) {
                builder.setReviewUrl(projectSite.getReviewUrl());
            }

            if (projectSite.getLibraryUrl() != null) {
                builder.setLibraryUrl(projectSite.getLibraryUrl());
            }

            if (projectSite.getAttachmentUrl() != null) {
                builder.setAttachmentUrl(magazineClient.createReadUrl(projectSite.getAttachmentUrl()));
            }

            Set<AppStoreReference> appStoreReferences = projectSite.getAppStoreReferences();
            if (appStoreReferences != null) {
                for (AppStoreReference appStoreReference : appStoreReferences) {
                    Protocol.ProjectSite.AppStoreReference.Builder appStoreReferenceBuilder = Protocol.ProjectSite.AppStoreReference.newBuilder();
                    appStoreReferenceBuilder.setId(appStoreReference.getId());
                    appStoreReferenceBuilder.setLabel(appStoreReference.getLabel());
                    appStoreReferenceBuilder.setUrl(appStoreReference.getUrl());
                    builder.addAppStoreReferences(appStoreReferenceBuilder.build());
                }
            }

            Set<Screenshot> screenShots = projectSite.getScreenshots();
            if (screenShots != null) {
                for (Screenshot screenShot : screenShots) {
                    Protocol.ProjectSite.Screenshot.Builder screenShotBuilder = Protocol.ProjectSite.Screenshot.newBuilder();
                    screenShotBuilder.setId(screenShot.getId());

                    if (screenShot.getMediaType() == Screenshot.MediaType.IMAGE) {
                        // Sign URL for images hosted by Magazine
                        screenShotBuilder.setUrl(magazineClient.createReadUrl(screenShot.getUrl()));
                    } else {
                        screenShotBuilder.setUrl(screenShot.getUrl());
                    }
                    screenShotBuilder.setMediaType(Protocol.ScreenshotMediaType.valueOf(screenShot.getMediaType().name()));
                    builder.addScreenshots(screenShotBuilder.build());
                }
            }
        }

        return builder.build();
    }
}
